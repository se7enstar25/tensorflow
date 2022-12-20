/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "gml_st/IR/gml_st_ops.h"
#include "gml_st/interfaces/tiling_interface_impl.h"
#include "gml_st/transforms/fusion/fusion.h"
#include "gml_st/transforms/passes.h"
#include "gml_st/transforms/peeling/peeling.h"
#include "gml_st/transforms/tiling/tiling.h"
#include "gml_st/transforms/transforms.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/Linalg/Transforms/Transforms.h"
#include "mlir/Dialect/Linalg/Utils/Utils.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

namespace mlir::gml_st {
namespace {

#define GEN_PASS_DEF_TRANSFORMMATMULFORCPUPASS
#include "gml_st/transforms/passes.h.inc"

static constexpr llvm::StringRef kMatmulTransformedLabel =
    "__matmul_transformed_label__";

// Helper to pick the tile shapes to use as the 2 inner dimensions of the
// 4D shapes appearing in a Mmt4D.
class Mmt4DTileParams {
 public:
  Mmt4DTileParams(ArrayRef<int> m0k0n0, const llvm::StringRef comment)
      : m0(m0k0n0[0]), k0(m0k0n0[1]), n0(m0k0n0[2]), comment(comment) {}
  std::array<int64_t, 2> lhs() const { return {m0, k0}; }
  std::array<int64_t, 2> rhs() const { return {k0, n0}; }
  std::array<int64_t, 2> acc() const { return {m0, n0}; }
  const std::string &getComment() const { return comment; }

 private:
  const int64_t m0;
  const int64_t k0;
  const int64_t n0;
  const std::string comment;
};

// Expands a 2D tensor input to a 4D tensor representing the same underlying
// data but now in a tiled layout, given a static 2D tile shape.
// Does not transpose.
// Example: (M, N) --> (M1, m0, N1, n0)
Value expandTo4D(mlir::Location loc, PatternRewriter &rewriter, Value input,
                 ArrayRef<int64_t> tileShape) {
  auto inputType = input.getType().cast<RankedTensorType>();
  ArrayRef<int64_t> inputShape = inputType.getShape();
  std::array<int64_t, 4> targetShape;
  // Generate a 4D shape of the form (M1, m0, N1, n0),
  // where m0, n0 are always static and M1, N1 are static if and only if M, N
  // are.
  for (int i : {0, 1}) {
    if (inputShape[i] == ShapedType::kDynamic) {
      targetShape[2 * i] = ShapedType::kDynamic;
    } else {
      targetShape[2 * i] = inputShape[i] / tileShape[i];
    }
    targetShape[2 * i + 1] = tileShape[i];
  }
  RankedTensorType targetType =
      RankedTensorType::get(targetShape, inputType.getElementType());
  std::array<ReassociationIndices, 2> expandIndices = {
      ReassociationIndices{0, 1}, ReassociationIndices{2, 3}};
  Value reshapedOperand = rewriter.create<tensor::ExpandShapeOp>(
      loc, targetType, input, expandIndices);
  return reshapedOperand;
}

// Creates a linalg.generic that transposes input using permutation indices.
// Example: (M1, m0, N1, n0) -> (M1, N1, m0, n0) if indices = {0, 2, 1, 3}.
Value transpose(mlir::Location loc, PatternRewriter &rewriter, Value input,
                ArrayRef<int64_t> indices) {
  auto inputType = input.getType().cast<RankedTensorType>();
  auto nloops = indices.size();

  SmallVector<AffineExpr, 4> exprs = llvm::to_vector<4>(
      llvm::map_range(indices, [&](int64_t index) -> AffineExpr {
        return rewriter.getAffineDimExpr(index);
      }));

  ArrayRef<int64_t> inputShape = inputType.getShape();
  SmallVector<OpFoldResult, 4> targetShape;
  for (int i = 0; i < 4; i++) {
    if (inputShape[indices[i]] == ShapedType::kDynamic) {
      targetShape.emplace_back(
          rewriter.create<tensor::DimOp>(loc, input, indices[i]));
    } else {
      targetShape.push_back(rewriter.getIndexAttr(inputShape[indices[i]]));
    }
  }

  Value outputTensor = rewriter.create<tensor::EmptyOp>(
      loc, targetShape, inputType.getElementType());

  SmallVector<utils::IteratorType, 4> loopAttributeTypes(
      nloops, utils::IteratorType::parallel);

  SmallVector<AffineMap, 2> indexingMaps = {
      inversePermutation(
          AffineMap::get(nloops, 0, exprs, rewriter.getContext())),
      AffineMap::getMultiDimIdentityMap(nloops, rewriter.getContext())};

  auto transposedOp = rewriter.create<linalg::GenericOp>(
      loc, outputTensor.getType(),
      /*inputs=*/input, /*outputs=*/outputTensor, indexingMaps,
      loopAttributeTypes,
      [&](OpBuilder &nestedBuilder, Location nestedLoc, ValueRange args) {
        nestedBuilder.create<linalg::YieldOp>(nestedLoc, args[0]);
      });

  return transposedOp.getResult(0);
}

// Collapses a 4d tensor input to 2d given its target shape.
// Example: (M1, m0, N1, n0) -> (M, N)
Value collapseTo2D(mlir::Location loc, PatternRewriter &rewriter, Value input,
                   ArrayRef<int64_t> targetShape) {
  auto inputType = input.getType().cast<RankedTensorType>();
  auto targetType =
      RankedTensorType::get(targetShape, inputType.getElementType());
  std::array<ReassociationIndices, 2> collapseIndices = {
      ReassociationIndices{0, 1}, ReassociationIndices{2, 3}};
  Value reshapedOperand = rewriter.create<tensor::CollapseShapeOp>(
      loc, targetType, input, collapseIndices);
  return reshapedOperand;
}

// Returns true if an input of the given |inputShape| needs padding to
// ensure that its shape will be a multiple of |tileShape|. That's always true
// in the dynamic shape case.
bool needsPadding(ArrayRef<int64_t> inputShape, ArrayRef<int64_t> tileShape) {
  assert(inputShape.size() == tileShape.size());
  for (size_t i = 0; i < inputShape.size(); i++) {
    if (inputShape[i] == ShapedType::kDynamic) {
      return true;
    }
    if (inputShape[i] % tileShape[i] != 0) {
      return true;
    }
  }
  return false;
}

// Pads |input| on the bottom and on the right to the next multiple of
// |tileShape|.
Value pad(Location loc, PatternRewriter &rewriter, Value input,
          ArrayRef<int64_t> tileShape) {
  SmallVector<OpFoldResult, 2> lowPadding, highPadding;
  SmallVector<int64_t, 2> resultTypeShape;
  auto inputType = input.getType().cast<RankedTensorType>();
  ArrayRef<int64_t> inputShape = inputType.getShape();
  if (!needsPadding(inputShape, tileShape)) {
    return input;
  }
  int64_t rank = inputType.getRank();
  for (int64_t i = 0; i < rank; ++i) {
    // No 'low' padding i.e. no padding at the top and on the left.
    lowPadding.push_back(rewriter.getIndexAttr(0));
    // 'High' padding i.e. padding at the bottom and on the right, and the
    // result type shape, will be dynamic in any dimension if and only if the
    // input shape is.
    if (inputShape[i] == ShapedType::kDynamic) {
      resultTypeShape.push_back(ShapedType::kDynamic);
      // There only remains to compute the 'high' padding Value.
      auto add = [&](Value a, Value b) {
        return rewriter.create<arith::AddIOp>(loc, a, b);
      };
      auto sub = [&](Value a, Value b) {
        return rewriter.create<arith::SubIOp>(loc, a, b);
      };
      auto rem = [&](Value a, Value b) {
        return rewriter.create<arith::RemSIOp>(loc, a, b);
      };
      // Compare to the plainer distanceToNextMultipleOf in the static
      // dimension case below.
      auto distanceToNextMultipleOf = [&](Value a, Value b) {
        Value one = rewriter.create<arith::ConstantIndexOp>(loc, 1);
        Value bMinusOne = sub(b, one);
        return sub(bMinusOne, rem(add(a, bMinusOne), b));
      };
      Value inputDim = rewriter.create<tensor::DimOp>(loc, input, i);
      Value tileDim =
          rewriter.create<arith::ConstantIndexOp>(loc, tileShape[i]);
      Value padding = distanceToNextMultipleOf(inputDim, tileDim);
      highPadding.push_back(padding);
    } else {
      auto distanceToNextMultipleOf = [=](int64_t a, int64_t b) {
        int64_t bMinusOne = b - 1;
        return bMinusOne - ((a + bMinusOne) % b);
      };
      int64_t inputDim = inputShape[i];
      int64_t tileDim = tileShape[i];
      int64_t padding = distanceToNextMultipleOf(inputDim, tileDim);
      resultTypeShape.push_back(inputDim + padding);
      highPadding.push_back(rewriter.getIndexAttr(padding));
    }
  }
  Type elementType = inputType.getElementType();
  RankedTensorType resultType =
      RankedTensorType::get(resultTypeShape, elementType);
  Value padValue = rewriter.create<arith::ConstantOp>(
      loc, elementType, rewriter.getZeroAttr(elementType));
  return rewriter.create<tensor::PadOp>(loc, resultType, input, lowPadding,
                                        highPadding, padValue);
}

// Returns a top-left slice from |input| shaped like |likeWhat|.
Value extractSliceLike(Location loc, PatternRewriter &rewriter, Value input,
                       Value likeWhat) {
  SmallVector<OpFoldResult, 2> offsets, dims, strides;
  auto resultType = likeWhat.getType().cast<RankedTensorType>();
  int64_t rank = resultType.getRank();
  auto resultShape = likeWhat.getType().cast<ShapedType>().getShape();
  for (int i = 0; i < rank; ++i) {
    offsets.push_back(rewriter.getIndexAttr(0));
    strides.push_back(rewriter.getIndexAttr(1));
    if (resultShape[i] == ShapedType::kDynamic) {
      dims.emplace_back(rewriter.create<tensor::DimOp>(loc, likeWhat, i));
    } else {
      dims.push_back(rewriter.getIndexAttr(resultShape[i]));
    }
  }
  return rewriter.create<tensor::ExtractSliceOp>(loc, resultType, input,
                                                 offsets, dims, strides);
}

bool haveEqualShapeDim(Value x, Value y, int i) {
  return x.getType().cast<ShapedType>().getDimSize(i) ==
         y.getType().cast<ShapedType>().getDimSize(i);
}

// Pattern to convert linalg.matmul to an equivalent subgraph using
// linalg.mmt4d. Currently, m0, n0 and k0 (packing parameters, aka layout tiling
// parameters) are compile-time constants.
struct MatmulToMmt4dPattern : public OpRewritePattern<linalg::MatmulOp> {
  using OpRewritePattern<linalg::MatmulOp>::OpRewritePattern;

  explicit MatmulToMmt4dPattern(MLIRContext *context,
                                PatternBenefit benefit = 1)
      : OpRewritePattern<linalg::MatmulOp>(context, benefit) {}

  LogicalResult matchAndRewrite(linalg::MatmulOp matmulOp,
                                PatternRewriter &rewriter) const override {
    Location loc = matmulOp.getLoc();

    Value lhs = matmulOp.getDpsInputOperand(0)->get();
    Value rhs = matmulOp.getDpsInputOperand(1)->get();
    Value acc = matmulOp.getDpsInitOperand(0)->get();

    // This transformation supports any mixing of static and dynamic dimensions,
    // with one exception: the dynamic-ness of each dimension of the accumulator
    // must match the dynamic-ness of the corresponding lhs/rhs dimension.
    // This limitation is not inherent to this transformation's code, it's just
    // here to avoid a current linalg folding limitation: at the moment,
    // removing this gives the following error in e2e matmul tests,
    //   "error: failed to legalize operation 'tensor.cast' that was explicitly
    //   marked illegal"
    // apparently due to some missing folding of tensor.cast op into reshapes.
    if (!haveEqualShapeDim(lhs, acc, 0) || !haveEqualShapeDim(rhs, acc, 1)) {
      return failure();
    }

    ShapedType lhsType = lhs.getType().cast<ShapedType>();
    ShapedType rhsType = rhs.getType().cast<ShapedType>();
    int64_t shapeM = lhsType.getShape()[0];
    int64_t shapeN = rhsType.getShape()[1];
    auto chooseMatMulOrMatVec = [=](ArrayRef<int> m0k0n0,
                                    ArrayRef<int> m0k0n0ForMatVec,
                                    ArrayRef<int> m0k0n0ForWhenRhsHas2Columns,
                                    std::string comment) {
      assert(m0k0n0ForMatVec[2] == 1 && "not a matrix*vector shape");
      assert(m0k0n0ForWhenRhsHas2Columns[2] == 2 &&
             "N=2 is expected when RHS has 2 columns");

      SmallVector<int> params;
      if (shapeN == 1 || shapeM == 1) {
        params.assign(m0k0n0ForMatVec.begin(), m0k0n0ForMatVec.end());
      } else if (shapeN == 2 || shapeM == 2) {
        params.assign(m0k0n0ForWhenRhsHas2Columns.begin(),
                      m0k0n0ForWhenRhsHas2Columns.end());
      } else {
        return Mmt4DTileParams(m0k0n0, comment);
      }

      if (shapeN == 1 || shapeN == 2) {
        comment += ", matrix * narrow matrix, where the narrow matrix has " +
                   std::to_string(shapeN) + " column(s)";
      } else {
        // The vector*matrix case is intentionally derived from the
        // matrix*vector case by swapping M and N dims so that in kernel
        // codegen we can reuse matrix*vector kernels by swapping LHS and RHS.
        std::swap(params[0], params[2]);
        comment += ", narrow matrix * matrix, where the narrow matrix has " +
                   std::to_string(shapeM) + " column(s)";
      }
      return Mmt4DTileParams(params, comment);
    };

    const auto &tileParams = chooseMatMulOrMatVec(
        {8, 1, 8}, {8, 1, 1}, {8, 1, 2}, "f32*f32->f32, generic");

    Value paddedLhs = pad(loc, rewriter, lhs, tileParams.lhs());
    Value paddedRhs = pad(loc, rewriter, rhs, tileParams.rhs());
    Value paddedAcc = pad(loc, rewriter, acc, tileParams.acc());

    Value lhs4D = expandTo4D(loc, rewriter, paddedLhs, tileParams.lhs());
    Value rhs4D = expandTo4D(loc, rewriter, paddedRhs, tileParams.rhs());
    Value acc4D = expandTo4D(loc, rewriter, paddedAcc, tileParams.acc());

    Value lhs4DT = transpose(loc, rewriter, lhs4D, {0, 2, 1, 3});
    Value rhs4DT = transpose(loc, rewriter, rhs4D, {2, 0, 3, 1});
    Value acc4DT = transpose(loc, rewriter, acc4D, {0, 2, 1, 3});

    auto mmt4d = rewriter.create<linalg::Mmt4DOp>(
        loc, acc4DT.getType(), ValueRange{lhs4DT, rhs4DT}, ValueRange{acc4DT});
    mmt4d->setAttr(StringAttr::get(getContext(), "comment"),
                   StringAttr::get(getContext(), tileParams.getComment()));

    Value mmt4dResultTransposed =
        transpose(loc, rewriter, mmt4d.getResult(0), {0, 2, 1, 3});

    Value paddedResult =
        collapseTo2D(loc, rewriter, mmt4dResultTransposed,
                     paddedAcc.getType().cast<ShapedType>().getShape());
    Value result = extractSliceLike(loc, rewriter, paddedResult, acc);

    rewriter.replaceOp(matmulOp, ArrayRef<Value>{result});

    return success();
  }
};

/// Canonicalizes [tensor.empty() -> linalg.fill -> linalg.generic] ->
/// [tensor.empty() -> linalg.fill] where linalg.generic does only copy e.g
/// a transpose.
struct FoldFillGenericOpPattern : public OpRewritePattern<linalg::GenericOp> {
  using OpRewritePattern<linalg::GenericOp>::OpRewritePattern;

  explicit FoldFillGenericOpPattern(MLIRContext *context,
                                    PatternBenefit benefit = 1)
      : OpRewritePattern<linalg::GenericOp>(context, benefit) {}

  LogicalResult matchAndRewrite(linalg::GenericOp genericOp,
                                PatternRewriter &rewriter) const override {
    if (genericOp.getNumDpsInputs() != 1) return failure();
    if (genericOp.getNumDpsInits() != 1) return failure();

    // Check linalg.generic does have copy only semantics.
    if (genericOp.getNumParallelLoops() != genericOp.getNumLoops()) {
      return failure();
    }
    auto results =
        llvm::to_vector<4>(genericOp.getBody()->getOps<linalg::YieldOp>());
    if (results.size() != 1) return failure();
    if (results[0].getValues().size() != 1) return failure();
    auto blockArgument = results[0].getValues()[0].dyn_cast<BlockArgument>();
    if (!blockArgument || blockArgument.getArgNumber() != 0) return failure();

    auto input = genericOp.getInputs()[0];

    auto outputType =
        genericOp.getOutputs()[0].getType().dyn_cast<RankedTensorType>();

    // FIXME: To enable dynamic shapes we need to apply the same permutation on
    // init tensor sizes.
    if (!outputType || !outputType.hasStaticShape()) return failure();

    auto fillOp = dyn_cast<linalg::FillOp>(input.getDefiningOp());
    if (!fillOp) return failure();

    auto loc = genericOp.getLoc();
    Value newInitTensor = rewriter.create<tensor::EmptyOp>(
        loc, outputType.getShape(), outputType.getElementType());
    rewriter.replaceOpWithNewOp<linalg::FillOp>(genericOp, fillOp.value(),
                                                newInitTensor);
    return success();
  }
};

/// Rewrite a tensor::PadOp into a sequence of EmptyOp, FillOp and
/// MapOp, which would be eligible for tiling/peeling and vectorization.
struct MapCopyPadOpPattern : public linalg::GeneralizePadOpPattern {
  explicit MapCopyPadOpPattern(MLIRContext *context, PatternBenefit benefit = 1)
      : linalg::GeneralizePadOpPattern(context, emitMapCopy, benefit) {}

  static LogicalResult emitMapCopy(PatternRewriter &rewriter,
                                   tensor::PadOp padOp, Value dest) {
    auto sourceType = padOp.getSourceType();
    auto destType = dest.getType().dyn_cast<ShapedType>();

    // TODO(vuson): add support for dynamic shape, which should also be
    // tiled/peeled/vectorized.
    if (!sourceType.hasStaticShape() || !destType) return failure();

    ArrayRef<int64_t> shape = sourceType.getShape();
    int64_t rank = sourceType.getRank();
    linalg::SliceParameters sliceParams;
    sliceParams.sizes.reserve(rank);
    sliceParams.strides =
        SmallVector<OpFoldResult>(rank, rewriter.getIndexAttr(1));
    for (unsigned r = 0; r < rank; ++r) {
      sliceParams.sizes.push_back(rewriter.getIndexAttr(shape[r]));
    }

    // Extract a slice of source's shape, which is the destination of the copy,
    // from the tensor to be padded.
    auto extractOp = rewriter.create<tensor::ExtractSliceOp>(
        padOp.getLoc(), dest, padOp.getMixedLowPad(), sliceParams.sizes,
        sliceParams.strides);

    // Perform copy from the source to the extracted slice.
    auto copy = rewriter.create<linalg::MapOp>(
        padOp.getLoc(), padOp.getSource(), extractOp.getResult(),
        [](OpBuilder &b, Location loc, ValueRange args) {
          b.create<linalg::YieldOp>(loc, args.front());
        });

    // Insert the extracted slice (with the source's data copied) back into the
    // tensor to be padded.
    rewriter.replaceOpWithNewOp<tensor::InsertSliceOp>(
        padOp, copy.getResult().front(), dest, padOp.getMixedLowPad(),
        sliceParams.sizes, sliceParams.strides);

    return success();
  }
};

FailureOr<TilingResult> tileMatmul(PatternRewriter &rewriter, Operation *op,
                                   ArrayRef<int64_t> tileSizes,
                                   bool distribute) {
  TilingOptions opts;
  opts.setTileSizeComputationFn(tileSizes);
  opts.distribute = distribute;
  return tile(opts, rewriter, cast<TilingInterface>(op));
}

/// Splits the tile sizes in `parallelSizes` into `reductionSizes` for the
/// reduction loops.
void splitParallelAndReductionTiles(linalg::LinalgOp op,
                                    SmallVectorImpl<int64_t> &parallelSizes,
                                    SmallVectorImpl<int64_t> &reductionSizes) {
  reductionSizes.assign(parallelSizes.begin(), parallelSizes.end());
  for (auto [index, iteratorType] :
       llvm::enumerate(op.getIteratorTypesArray())) {
    if (iteratorType == utils::IteratorType::parallel) {
      reductionSizes[index] = 0;
    } else {
      parallelSizes[index] = 0;
    }
  }
}

/// Pattern to tile `linalg.mmt4d`.
struct Mmt4DTransformPattern : public OpRewritePattern<linalg::Mmt4DOp> {
  using OpRewritePattern<linalg::Mmt4DOp>::OpRewritePattern;

  explicit Mmt4DTransformPattern(MLIRContext *context,
                                 PatternBenefit benefit = 1)
      : OpRewritePattern<linalg::Mmt4DOp>(context, benefit) {}

  LogicalResult matchAndRewrite(linalg::Mmt4DOp mmt4dOp,
                                PatternRewriter &rewriter) const override {
    if (hasLabel(mmt4dOp, kMatmulTransformedLabel)) {
      return rewriter.notifyMatchFailure(mmt4dOp,
                                         "has already been transformed.");
    }

    // Compute the tile sizes. Note that at this stage we only do layout tiling.
    // Later we might also want to do traversal tiling (only on M and N dims).
    auto getL1TileSizes = [&]() -> SmallVector<int64_t> {
      auto lhsShape =
          mmt4dOp.getInputs()[0].getType().cast<ShapedType>().getShape();
      auto rhsShape =
          mmt4dOp.getInputs()[1].getType().cast<ShapedType>().getShape();
      int64_t m0 = lhsShape[2];
      int64_t n0 = rhsShape[2];
      int64_t k0 = lhsShape[3];
      return {1, 1, 1, m0, n0, k0};
    };

    SmallVector<int64_t> parallelTileSizes = getL1TileSizes();
    SmallVector<int64_t> reductionTileSizes;

    // Search the number of outer parallel loops to separate them from possible
    // inner reduction dimensions.
    auto iterTypes = mmt4dOp.getIteratorTypesArray();
    // Make sure to only look at the leading loops for tiling---we will scan
    // this array to find the first non-parallel loop later and use that for
    // indexing into the tile sizes.
    if (iterTypes.size() > parallelTileSizes.size()) {
      iterTypes.resize(parallelTileSizes.size());
    }

    splitParallelAndReductionTiles(mmt4dOp.getOperation(), parallelTileSizes,
                                   reductionTileSizes);

    auto *it = find_if_not(iterTypes, linalg::isParallelIterator);
    int64_t split = std::distance(iterTypes.begin(), it);

    // Perform tiling in two steps.
    SmallVector<int64_t> outerTileSizes(parallelTileSizes.size(), 0);
    SmallVector<int64_t> innerTileSizes(parallelTileSizes.size(), 0);
    std::copy(parallelTileSizes.begin(), parallelTileSizes.begin() + split + 1,
              outerTileSizes.begin());
    std::copy(parallelTileSizes.begin() + split + 1, parallelTileSizes.end(),
              innerTileSizes.begin() + split + 1);

    // Tile the outer parallel loop.
    auto tilingParallelDimsResult =
        tileMatmul(rewriter, mmt4dOp, outerTileSizes, /*distribute=*/true);
    if (failed(tilingParallelDimsResult)) return failure();
    // Update the results if tiling occurred.
    if (tilingParallelDimsResult->loop != nullptr) {
      rewriter.replaceOp(mmt4dOp, tilingParallelDimsResult->loop->getResults());
      mmt4dOp =
          cast<linalg::Mmt4DOp>(tilingParallelDimsResult->tiledOps.front());
    }

    // Tile the inner parallel loop.
    tilingParallelDimsResult =
        tileMatmul(rewriter, mmt4dOp, innerTileSizes, /*distribute=*/true);
    if (failed(tilingParallelDimsResult)) return failure();
    // Update the results if tiling occurred.
    if (tilingParallelDimsResult->loop != nullptr) {
      rewriter.replaceOp(mmt4dOp, tilingParallelDimsResult->loop->getResults());
      mmt4dOp =
          cast<linalg::Mmt4DOp>(tilingParallelDimsResult->tiledOps.front());
    }

    std::copy(reductionTileSizes.begin(),
              reductionTileSizes.begin() + split + 1, outerTileSizes.begin());
    std::copy(reductionTileSizes.begin() + split + 1, reductionTileSizes.end(),
              innerTileSizes.begin() + split + 1);

    // Tile the outer reduction loop.
    auto tilingReductionDimsResult =
        tileMatmul(rewriter, mmt4dOp, outerTileSizes, /*distribute=*/false);
    if (failed(tilingReductionDimsResult)) return failure();
    // Update the results if tiling occurred.
    if (tilingReductionDimsResult->loop != nullptr) {
      rewriter.replaceOp(mmt4dOp,
                         tilingReductionDimsResult->loop->getResults());
      mmt4dOp =
          cast<linalg::Mmt4DOp>(tilingReductionDimsResult->tiledOps.front());
    }

    // Tile the inner reduction loop.
    tilingReductionDimsResult =
        tileMatmul(rewriter, mmt4dOp, innerTileSizes, /*distribute=*/false);
    if (failed(tilingReductionDimsResult)) return failure();
    // Update the results if tiling occurred.
    if (tilingReductionDimsResult->loop != nullptr) {
      rewriter.replaceOp(mmt4dOp,
                         tilingReductionDimsResult->loop->getResults());
      mmt4dOp =
          cast<linalg::Mmt4DOp>(tilingReductionDimsResult->tiledOps.front());
    }

    setLabel(mmt4dOp, kMatmulTransformedLabel);
    return success();
  }
};

/// Pattern to tile `linalg.matmul`, fuse `linalg.fill` into generated
/// `gml_st.parallel`, and peel the generated loops.
struct MatmulTransformPattern : public OpRewritePattern<linalg::MatmulOp> {
  using OpRewritePattern<linalg::MatmulOp>::OpRewritePattern;

  explicit MatmulTransformPattern(MLIRContext *context,
                                  int64_t lhsParallelDimTileSize = 2,
                                  int64_t rhsParallelDimTileSize = 4,
                                  int64_t reductionDimTileSize = 8,
                                  PatternBenefit benefit = 1)
      : OpRewritePattern<linalg::MatmulOp>(context, benefit),
        lhsParallelDimTileSize(lhsParallelDimTileSize),
        rhsParallelDimTileSize(rhsParallelDimTileSize),
        reductionDimTileSize(reductionDimTileSize) {}

  LogicalResult matchAndRewrite(linalg::MatmulOp matmulOp,
                                PatternRewriter &rewriter) const override {
    if (hasLabel(matmulOp, kMatmulTransformedLabel))
      return rewriter.notifyMatchFailure(matmulOp,
                                         "has already been transformed.");
    if (isa<gml_st::ParallelOp, gml_st::ForOp>(matmulOp->getParentOp()))
      return rewriter.notifyMatchFailure(
          matmulOp, "has already been tiled by another pass.");

    auto cluster = findMapFusionCluster(matmulOp);
    auto fusionCluster = cluster.operations;
    auto *tilingRoot = cluster.root;

    // Tiling of linalg.map requires two dimensions, linalg.matmul requires
    // three.
    SmallVector<int64_t> parallelDimsTileSizes{lhsParallelDimTileSize,
                                               rhsParallelDimTileSize};
    if (isa<linalg::MatmulOp>(tilingRoot)) parallelDimsTileSizes.push_back(0);

    // First level tiling: parallel dimensions.
    auto tilingParallelDimsResult = tileMatmul(
        rewriter, tilingRoot, parallelDimsTileSizes, /*distribute=*/true);
    if (failed(tilingParallelDimsResult)) return failure();

    // Update the results if tiling occurred.
    if (tilingParallelDimsResult->loop != nullptr) {
      rewriter.replaceOp(tilingRoot,
                         tilingParallelDimsResult->loop->getResults());
      tilingRoot = tilingParallelDimsResult->tiledOps.front();

      // Fuse ops into the loop.
      fuseGreedily(rewriter, *tilingRoot->getBlock(),
                   [&](Operation *op) { return fusionCluster.contains(op); });
    }

    // Second level tiling: reduction dimension for matmuls.
    SmallVector<TilingResult> tilingReductionDimsResults;
    for (auto op :
         llvm::to_vector(tilingRoot->getBlock()->getOps<linalg::MatmulOp>())) {
      // Fusion into the output.
      if (failed(fuseOutputFill(rewriter, op))) return failure();

      auto result = tileMatmulReductionDims(rewriter, op);
      if (failed(result)) return failure();
      tilingReductionDimsResults.push_back(result.value());
    }

    // Peel parallel loops.
    //
    // We only want to peel (1) the parallel loop then (2) our kernel.
    if (auto loop =
            dyn_cast_or_null<ParallelOp>(tilingParallelDimsResult->loop)) {
      auto peelingResult = peelAllLoops(loop, rewriter);
    }

    // Peel reduction loop inside the main parallel loop, label the main loop as
    // "perfectly tiled" one, to enable vectorization after canonicalization.
    for (auto res : tilingReductionDimsResults) {
      if (auto loop = dyn_cast_or_null<ForOp>(res.loop)) {
        auto peelingResult = peelAllLoops(loop, rewriter);
        setLabel(loop, kPerfectlyTiledLoopLabel);
      }
    }

    return success();
  }

 private:
  FailureOr<TilingResult> tileMatmulReductionDims(
      PatternRewriter &rewriter, linalg::MatmulOp matmulOp) const {
    SmallVector<int64_t> reductionDimsTileSizes{0, 0, reductionDimTileSize};
    auto tilingReductionDimsResult = tileMatmul(
        rewriter, matmulOp, reductionDimsTileSizes, /*distribute=*/false);
    if (failed(tilingReductionDimsResult)) return failure();

    // Update the results if tiling occurred.
    if (tilingReductionDimsResult->loop != nullptr) {
      rewriter.replaceOp(matmulOp,
                         tilingReductionDimsResult->loop->getResults());
      matmulOp =
          cast<linalg::MatmulOp>(tilingReductionDimsResult->tiledOps.front());
    }

    setLabel(matmulOp, kMatmulTransformedLabel);
    return tilingReductionDimsResult;
  }

  int64_t lhsParallelDimTileSize;
  int64_t rhsParallelDimTileSize;
  int64_t reductionDimTileSize;
};

struct TransformMatmulForCpuPass
    : public impl::TransformMatmulForCpuPassBase<TransformMatmulForCpuPass> {
  TransformMatmulForCpuPass() = default;

  explicit TransformMatmulForCpuPass(llvm::ArrayRef<int64_t> matmulTileSizes,
                                     bool lowerToMmt4DOp) {
    tileSizes = matmulTileSizes;
    lowerToMmt4D = lowerToMmt4DOp;
  }

  void getDependentDialects(DialectRegistry &registry) const final {
    registry.insert<mlir::gml_st::GmlStDialect, arith::ArithDialect,
                    linalg::LinalgDialect, tensor::TensorDialect>();
    mlir::gml_st::registerGmlStTilingInterfaceExternalModels(registry);
  }

  void runOnOperation() override {
    func::FuncOp f = getOperation();
    MLIRContext *ctx = &getContext();

    // Just do tiling and fusion on linalg.matmul.
    if (!lowerToMmt4D) {
      if (tileSizes.empty()) {
        tileSizes = {4, 4, 4};
      }
      assert(tileSizes.size() == 3 &&
             "Tiling sizes for MatMul should have 3 elements");
      RewritePatternSet patterns(ctx);
      patterns.add<MatmulTransformPattern>(ctx, tileSizes[0], tileSizes[1],
                                           tileSizes[2]);
      if (failed(applyPatternsAndFoldGreedily(f, std::move(patterns)))) {
        return signalPassFailure();
      }
      // Ensure we drop the marker in the end.
      f.walk([](linalg::MatmulOp op) {
        removeLabel(op, kMatmulTransformedLabel);
      });
      return;
    }

    // Lower linalg.matmul to linalg.mmt4d (packed matmul).
    {
      // Convert linalg.matmul to linalg.mmt4d.
      RewritePatternSet patterns(ctx);
      patterns.add<MatmulToMmt4dPattern>(ctx);

      // Canonicalization.
      tensor::ExpandShapeOp::getCanonicalizationPatterns(patterns, ctx);
      tensor::EmptyOp::getCanonicalizationPatterns(patterns, ctx);
      linalg::FillOp::getCanonicalizationPatterns(patterns, ctx);
      patterns.add<FoldFillGenericOpPattern>(ctx);

      // Lower tensor.pad to linalg.map.
      patterns.add<MapCopyPadOpPattern>(ctx);

      if (failed(applyPatternsAndFoldGreedily(f, std::move(patterns)))) {
        return signalPassFailure();
      }
      // Ensure we drop the marker in the end.
      f.walk([](Operation *op) {
        if (isa<linalg::MatmulOp>(op) || isa<linalg::Mmt4DOp>(op))
          removeLabel(op, kMatmulTransformedLabel);
      });
    }
    // Tiling.
    {
      RewritePatternSet patterns(ctx);
      // We tile towards SIMD codegen, so the tile sizes depend on the target
      // architecture (vector instruction sizes, etc.). Luckily, this
      // information is already captured in linalg.mmt4d during linalg.matmul ->
      // linalg.mmt4d lowering phase. It is hardcoded for AVX on x86 for now.
      patterns.add<Mmt4DTransformPattern>(ctx);

      if (failed(applyPatternsAndFoldGreedily(f, std::move(patterns)))) {
        return signalPassFailure();
      }
      // Ensure we drop the marker in the end.
      f.walk(
          [](linalg::Mmt4DOp op) { removeLabel(op, kMatmulTransformedLabel); });
    }
  }
};
}  // namespace

std::unique_ptr<mlir::OperationPass<mlir::func::FuncOp>>
createTransformMatmulForCpuPass() {
  return std::make_unique<mlir::gml_st::TransformMatmulForCpuPass>();
}

std::unique_ptr<mlir::OperationPass<mlir::func::FuncOp>>
createTransformMatmulForCpuPass(llvm::ArrayRef<int64_t> matmulTileSizes,
                                bool lowerToMmt4DOp) {
  return std::make_unique<mlir::gml_st::TransformMatmulForCpuPass>(
      matmulTileSizes, lowerToMmt4DOp);
}

}  // namespace mlir::gml_st
