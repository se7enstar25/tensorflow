/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

// This file implements logic for legalizing HLO to TensorFlow.

#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <vector>

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"  // from @llvm-project
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/Matchers.h"  // from @llvm-project
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/IR/StandardTypes.h"  // from @llvm-project
#include "mlir/IR/Value.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "mlir/Transforms/DialectConversion.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/hlo/include/mlir-hlo/Dialect/mhlo/IR/chlo_ops.h"
#include "tensorflow/compiler/mlir/hlo/include/mlir-hlo/Dialect/mhlo/IR/hlo_ops.h"
#include "tensorflow/compiler/mlir/hlo/include/mlir-hlo/utils/broadcast_utils.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/passes.h"
#include "tensorflow/core/framework/kernel_shape_util.h"
#include "tensorflow/core/lib/math/math_util.h"

namespace mlir {
namespace TF {
namespace {

using mhlo::DotDimensionNumbers;

class ConvertConvOp : public OpConversionPattern<mhlo::ConvOp> {
 public:
  using OpConversionPattern::OpConversionPattern;

  LogicalResult matchAndRewrite(
      mhlo::ConvOp conv_op, ArrayRef<Value> args,
      ConversionPatternRewriter &rewriter) const final {
    if (!IsSupportedConvOp(conv_op)) {
      return failure();
    }

    // Constructs strides array.
    // For example, [2, 3] -> [1, 2, 3, 1].
    SmallVector<int64_t, 4> strides({1});
    for (const auto v :
         conv_op.window_strides().getValue().getValues<int64_t>()) {
      strides.emplace_back(v);
    }
    strides.emplace_back(1);

    // Constructs dilation array.
    SmallVector<int64_t, 4> dilation;
    if (auto rhs_dilation = conv_op.rhs_dilation()) {
      // For example, [2, 3] -> [1, 2, 3, 1].
      dilation.emplace_back(1);
      dilation.append(rhs_dilation.getValue().getValues<int64_t>().begin(),
                      rhs_dilation.getValue().getValues<int64_t>().end());
      dilation.emplace_back(1);
    } else {
      // Default value
      dilation = {1, 1, 1, 1};
    }

    const int input_feature_dimension =
        conv_op.dimension_numbers().input_feature_dimension().getInt();
    const int input_channels =
        conv_op.lhs().getType().cast<ShapedType>().getDimSize(
            input_feature_dimension);
    int feature_group_count = conv_op.feature_group_count();

    const bool is_depthwise_conv = input_channels == feature_group_count;
    std::string padding;

    if (!conv_op.padding().hasValue() ||
        (conv_op.padding().getValue().isSplat() &&
         conv_op.padding()->getSplatValue<int64_t>() == 0)) {
      padding = "VALID";
    } else {
      // Check if padding is "SAME".
      // TODO(chhe): To support "EXPLICIT" padding.
      SmallVector<int64_t, 8> padding_array;
      for (const auto v : conv_op.padding().getValue().getValues<int64_t>()) {
        padding_array.emplace_back(v);
      }

      const int num_spatial_dims = conv_op.dimension_numbers()
                                       .input_spatial_dimensions()
                                       .getNumElements();
      if (!IsSamePadding(conv_op, num_spatial_dims, strides, dilation,
                         padding_array))
        return failure();

      padding = "SAME";
    }

    CreateConvOp(conv_op, strides, padding, dilation, is_depthwise_conv,
                 rewriter);
    return success();
  };

 private:
  bool IsSamePadding(mhlo::ConvOp conv_op, int num_spatial_dims,
                     ArrayRef<int64_t> strides, ArrayRef<int64_t> dilation,
                     ArrayRef<int64_t> padding_array) const {
    for (auto i : llvm::seq<int>(0, num_spatial_dims)) {
      int dim = i + 1;
      tensorflow::int64 output_size;
      tensorflow::int64 pad_low_int64;
      tensorflow::int64 pad_high_int64;
      tensorflow::Status status = tensorflow::GetWindowedOutputSizeVerboseV2(
          conv_op.lhs().getType().cast<ShapedType>().getDimSize(dim),
          conv_op.rhs().getType().cast<ShapedType>().getDimSize(i),
          dilation[dim], strides[dim], tensorflow::Padding::SAME, &output_size,
          &pad_low_int64, &pad_high_int64);
      if (!status.ok()) return false;
      if (padding_array[2 * i] != pad_low_int64 ||
          padding_array[2 * i + 1] != pad_high_int64)
        return false;
    }

    return true;
  }

  void CreateConvOp(mhlo::ConvOp conv_op, ArrayRef<int64_t> strides,
                    StringRef padding, ArrayRef<int64_t> dilation,
                    bool is_depthwise_conv,
                    ConversionPatternRewriter &rewriter) const {
    // TODO(chhe): To support more data formats other than "NHWC".
    if (is_depthwise_conv) {
      rewriter.replaceOpWithNewOp<DepthwiseConv2dNativeOp>(
          conv_op, conv_op.getType(), conv_op.lhs(), conv_op.rhs(),
          rewriter.getI64ArrayAttr(strides),
          /*padding=*/rewriter.getStringAttr(padding),
          /*explicit_paddings=*/rewriter.getI64ArrayAttr({}),
          /*data_format=*/rewriter.getStringAttr("NHWC"),
          /*dilations=*/rewriter.getI64ArrayAttr(dilation));
    } else {
      rewriter.replaceOpWithNewOp<Conv2DOp>(
          conv_op, conv_op.getType(), conv_op.lhs(), conv_op.rhs(),
          rewriter.getI64ArrayAttr(strides),
          /*use_cudnn_on_gpu=*/rewriter.getBoolAttr(true),
          /*padding=*/rewriter.getStringAttr(padding),
          /*explicit_paddings=*/rewriter.getI64ArrayAttr({}),
          /*data_format=*/rewriter.getStringAttr("NHWC"),
          /*dilations=*/rewriter.getI64ArrayAttr(dilation));
    }
  }

  bool IsSupportedConvOp(mhlo::ConvOp conv_op) const {
    if (!conv_op.lhs().getType().cast<ShapedType>().hasStaticShape() ||
        !conv_op.rhs().getType().cast<ShapedType>().hasStaticShape() ||
        !conv_op.getType().cast<ShapedType>().hasStaticShape())
      return false;

    // All ones in "lhs_dilation" means this "mhlo.conv" op should be
    // converted to "tf.Conv2D" or "tf.DepthwiseConv2dNativeOp".
    if (conv_op.lhs_dilation().hasValue()) {
      auto lhs_dilation = conv_op.lhs_dilation().getValue();
      if (!lhs_dilation.isSplat() || lhs_dilation.getSplatValue<int64_t>() != 1)
        return false;
    }

    if (!conv_op.window_strides().hasValue() || conv_op.window_strides()
                                                        .getValue()
                                                        .getType()
                                                        .cast<ShapedType>()
                                                        .getRank() != 1)
      return false;

    int num_spatial_dims =
        conv_op.dimension_numbers().input_spatial_dimensions().getNumElements();
    // TODO(b/158636600): Currently we don't support 3D Convolution.
    if (num_spatial_dims != 2) return false;

    // TODO(chhe): To support more data formats other than "NHWC".
    // Checks input dimensions.
    if (conv_op.dimension_numbers().input_batch_dimension().getInt() != 0 ||
        conv_op.dimension_numbers().input_feature_dimension().getInt() !=
            num_spatial_dims + 1)
      return false;
    DenseIntElementsAttr input_spatial_dimensions =
        conv_op.dimension_numbers().input_spatial_dimensions();
    for (auto p :
         llvm::enumerate(input_spatial_dimensions.getValues<int64_t>())) {
      if (p.value() != p.index() + 1) return false;
    }

    // Checks output dimensions.
    if (conv_op.dimension_numbers().output_batch_dimension().getInt() != 0 ||
        conv_op.dimension_numbers().output_feature_dimension().getInt() !=
            num_spatial_dims + 1)
      return false;
    DenseIntElementsAttr output_spatial_dimensions =
        conv_op.dimension_numbers().output_spatial_dimensions();
    for (auto p :
         llvm::enumerate(output_spatial_dimensions.getValues<int64_t>())) {
      if (p.value() != p.index() + 1) return false;
    }

    // Checks kernel dimensions.
    if (conv_op.dimension_numbers().kernel_input_feature_dimension().getInt() !=
            num_spatial_dims ||
        conv_op.dimension_numbers()
                .kernel_output_feature_dimension()
                .getInt() != num_spatial_dims + 1)
      return false;
    DenseIntElementsAttr kernal_spatial_dimensions =
        conv_op.dimension_numbers().kernel_spatial_dimensions();
    for (auto p :
         llvm::enumerate(kernal_spatial_dimensions.getValues<int64_t>())) {
      if (p.value() != p.index()) return false;
    }

    return true;
  }
};

class ConvertSliceOp : public OpConversionPattern<mhlo::SliceOp> {
 public:
  using OpConversionPattern::OpConversionPattern;

  LogicalResult matchAndRewrite(
      mhlo::SliceOp slice_op, ArrayRef<Value> args,
      ConversionPatternRewriter &rewriter) const final {
    DenseIntElementsAttr strides = slice_op.strides();
    // Strides must be 1 otherwise we cannot legalize this `mhlo.slice` op.
    if (!strides.isSplat() ||
        strides.getSplatValue().cast<IntegerAttr>().getInt() != 1)
      return failure();

    rewriter.setInsertionPointAfter(slice_op.getOperation());
    auto start_indices = slice_op.start_indices();
    auto limit_indices = slice_op.limit_indices();
    std::vector<int64_t> size_values;
    for (auto pair : llvm::zip(start_indices.getValues<APInt>(),
                               limit_indices.getValues<APInt>())) {
      size_values.emplace_back(std::get<1>(pair).getSExtValue() -
                               std::get<0>(pair).getSExtValue());
    }

    RankedTensorType ty =
        RankedTensorType::get({static_cast<int64_t>(size_values.size())},
                              rewriter.getIntegerType(64));
    auto start = rewriter.create<ConstOp>(slice_op.getLoc(), start_indices);
    auto size = rewriter.create<ConstOp>(
        slice_op.getLoc(), DenseIntElementsAttr::get(ty, size_values));
    rewriter.replaceOpWithNewOp<SliceOp>(slice_op, slice_op.getType(),
                                         slice_op.operand(), start, size);
    return success();
  };
};

// Appends all elements in `range` to `values`.
template <typename ValueT, typename Range>
void Append(llvm::SmallVectorImpl<ValueT> &values, Range &&range) {
  values.insert(values.end(), range.begin(), range.end());
}

// Appends all elements in `range` to `values`.
template <typename ValueT, typename Range, typename... RangeTs>
void Append(llvm::SmallVectorImpl<ValueT> &values, Range &&range,
            RangeTs &&...ranges) {
  values.insert(values.end(), range.begin(), range.end());
  Append(values, ranges...);
}

// Returns the number of elements in `range`.
template <typename Range>
size_t Size(Range &&range) {
  return range.size();
}

// Returns the total number of elements in a variadic number of `ranges`.
template <typename Range, typename... RangeTs>
size_t Size(Range &&range, RangeTs &&...ranges) {
  return range.size() + Size(std::forward<RangeTs>(ranges)...);
}

// Concats all elements in `ranges` and returns a small vector as a result.
template <typename ValueT, typename... RangeTs>
llvm::SmallVector<ValueT, 4> Concat(RangeTs &&...ranges) {
  llvm::SmallVector<int64_t, 4> results;
  results.reserve(Size(std::forward<RangeTs>(ranges)...));
  Append(results, std::forward<RangeTs>(ranges)...);
  return results;
}

// A struct to hold axes and sizes for a set of dimensions.
struct DimensionSetVector {
  llvm::ArrayRef<int64_t> AxesArray() const { return axes.getArrayRef(); }
  llvm::ArrayRef<int64_t> SizesArray() const { return sizes.getArrayRef(); }

  llvm::SmallSetVector<int64_t, 4> axes;
  llvm::SmallSetVector<int64_t, 4> sizes;
};

// A struct to hold information about dimensions of dot_general operands.
class DotDimensionsInfo {
 public:
  DotDimensionsInfo(ShapedType type, DenseIntElementsAttr batch_dimensions,
                    DenseIntElementsAttr contracting_dimensions) {
    const int rank = type.getRank();
    for (const int dim : batch_dimensions.getValues<int64_t>()) {
      batch_dimensions_.axes.insert(dim);
      batch_dimensions_.sizes.insert(type.getDimSize(dim));
    }

    for (const int dim : contracting_dimensions.getValues<int64_t>()) {
      contracting_dimensions_.axes.insert(dim);
      contracting_dimensions_.sizes.insert(type.getDimSize(dim));
    }

    for (int dim = 0; dim < rank; ++dim) {
      if (contracting_dimensions_.axes.count(dim) > 0 ||
          batch_dimensions_.axes.count(dim) > 0) {
        continue;
      }
      out_dimensions_.axes.insert(dim);
      out_dimensions_.sizes.insert(type.getDimSize(dim));
    }
  }

  const DimensionSetVector &batch_dimensions() const {
    return batch_dimensions_;
  }
  const DimensionSetVector &contracting_dimensions() const {
    return contracting_dimensions_;
  }
  // Out dimensions are any dimensions that are neither batch nor contracting
  // dimensions, hence will be propagated to output shape.
  const DimensionSetVector &out_dimensions() const { return out_dimensions_; }

  // Returns the total dimension size after flattening all contracting
  // dimensions.
  int FlattenedContractingDimensionSize() const {
    return std::accumulate(contracting_dimensions_.sizes.begin(),
                           contracting_dimensions_.sizes.end(), 1,
                           std::multiplies<int64_t>());
  }

  // Returns the total dimension size after flattening all out dimensions.
  int FlattenedOutDimensionSize() const {
    return std::accumulate(out_dimensions_.sizes.begin(),
                           out_dimensions_.sizes.end(), 1,
                           std::multiplies<int64_t>());
  }

 private:
  DimensionSetVector batch_dimensions_;
  DimensionSetVector contracting_dimensions_;
  // Out dimensions are any dimensions that are neither batch nor contracting
  // dimensions, hence will be propagated to output shape.
  DimensionSetVector out_dimensions_;
};

// Converts mhlo.dot to tf.BatchMatMul. Reshape or Transpose ops will also be
// inserted to convert to well-formed matrix multiply.
Value ConvertDotGeneralOp(PatternRewriter &rewriter, Operation *old_op) {
  auto dot_general_op = cast<mhlo::DotGeneralOp>(old_op);
  auto lhs_type = dot_general_op.lhs().getType().cast<ShapedType>();
  auto rhs_type = dot_general_op.rhs().getType().cast<ShapedType>();
  auto result_type = dot_general_op.getResult().getType().cast<ShapedType>();
  DotDimensionNumbers dot_dimension_numbers =
      dot_general_op.dot_dimension_numbers();
  mlir::Location loc = dot_general_op.getLoc();
  const int lhs_rank = lhs_type.getRank();
  const int rhs_rank = rhs_type.getRank();

  // Collects lhs and rhs dimensions information.
  DotDimensionsInfo lhs_dot_dimensions_info(
      lhs_type, dot_dimension_numbers.lhs_batching_dimensions(),
      dot_dimension_numbers.lhs_contracting_dimensions());
  DotDimensionsInfo rhs_dot_dimensions_info(
      rhs_type, dot_dimension_numbers.rhs_batching_dimensions(),
      dot_dimension_numbers.rhs_contracting_dimensions());

  // Transposes lhs shape to be in the order of {batch_dimensions,
  // out_dimensions, contracting dimensions}.
  llvm::SmallVector<int64_t, 4> lhs_permutation = Concat<int64_t>(
      lhs_dot_dimensions_info.batch_dimensions().AxesArray(),
      lhs_dot_dimensions_info.out_dimensions().AxesArray(),
      lhs_dot_dimensions_info.contracting_dimensions().AxesArray());
  llvm::SmallVector<int64_t, 4> lhs_transposed_shape = Concat<int64_t>(
      lhs_dot_dimensions_info.batch_dimensions().SizesArray(),
      lhs_dot_dimensions_info.out_dimensions().SizesArray(),
      lhs_dot_dimensions_info.contracting_dimensions().SizesArray());
  auto lhs_transposed = rewriter.create<mhlo::TransposeOp>(
      loc,
      RankedTensorType::get(lhs_transposed_shape, lhs_type.getElementType()),
      dot_general_op.lhs(),
      DenseIntElementsAttr::get(
          RankedTensorType::get({lhs_rank}, rewriter.getI64Type()),
          lhs_permutation));

  // Transposes rhs shape to be in the order of {batch_dimensions, contracting
  // dimensions, out_dimensions}.
  llvm::SmallVector<int64_t, 4> rhs_permutation = Concat<int64_t>(
      rhs_dot_dimensions_info.batch_dimensions().AxesArray(),
      rhs_dot_dimensions_info.contracting_dimensions().AxesArray(),
      rhs_dot_dimensions_info.out_dimensions().AxesArray());
  llvm::SmallVector<int64_t, 4> rhs_transposed_shape = Concat<int64_t>(
      rhs_dot_dimensions_info.batch_dimensions().SizesArray(),
      rhs_dot_dimensions_info.contracting_dimensions().SizesArray(),
      rhs_dot_dimensions_info.out_dimensions().SizesArray());
  auto rhs_transposed = rewriter.create<mhlo::TransposeOp>(
      loc,
      RankedTensorType::get(rhs_transposed_shape, rhs_type.getElementType()),
      dot_general_op.rhs(),
      DenseIntElementsAttr::get(
          RankedTensorType::get({rhs_rank}, rewriter.getI64Type()),
          rhs_permutation));

  // Reshapes lhs to flatten out_dimensions and contracting_dimensions.
  llvm::SmallVector<int64_t, 4> lhs_flattened_shape = Concat<int64_t>(
      lhs_dot_dimensions_info.batch_dimensions().SizesArray(),
      llvm::ArrayRef<int64_t>{
          lhs_dot_dimensions_info.FlattenedOutDimensionSize()},
      llvm::ArrayRef<int64_t>{
          lhs_dot_dimensions_info.FlattenedContractingDimensionSize()});
  auto lhs_flattend = rewriter.create<mhlo::ReshapeOp>(
      loc,
      RankedTensorType::get(lhs_flattened_shape, lhs_type.getElementType()),
      lhs_transposed.getResult());

  // Reshapes rhs to flatten out_dimensions and contracting_dimensions.
  llvm::SmallVector<int64_t, 4> rhs_flattened_shape = Concat<int64_t>(
      rhs_dot_dimensions_info.batch_dimensions().SizesArray(),
      llvm::ArrayRef<int64_t>{
          rhs_dot_dimensions_info.FlattenedContractingDimensionSize()},
      llvm::ArrayRef<int64_t>{
          rhs_dot_dimensions_info.FlattenedOutDimensionSize()});
  auto rhs_flattend = rewriter.create<mhlo::ReshapeOp>(
      loc,
      RankedTensorType::get(rhs_flattened_shape, rhs_type.getElementType()),
      rhs_transposed.getResult());

  // Creates matmul op of `lhs_flattend` and `rhs_flattend`.
  llvm::SmallVector<int64_t, 4> matmul_shape =
      Concat<int64_t>(lhs_dot_dimensions_info.batch_dimensions().SizesArray(),
                      llvm::ArrayRef<int64_t>{
                          lhs_dot_dimensions_info.FlattenedOutDimensionSize()},
                      llvm::ArrayRef<int64_t>{
                          rhs_dot_dimensions_info.FlattenedOutDimensionSize()});
  auto matmul = rewriter.create<TF::BatchMatMulV2Op>(
      loc, RankedTensorType::get(matmul_shape, result_type.getElementType()),
      lhs_flattend.getResult(), rhs_flattend.getResult());
  auto reshaped =
      rewriter.create<mhlo::ReshapeOp>(loc, result_type, matmul.getResult());
  return reshaped.getResult();
}

// Checks if the specified region is a binary reduction function what takes 2
// inputs, passes it to an instance of the specifiied reduction op and then
// returns the result.
template <typename ReductionOp>
LogicalResult MatchBinaryReduceFunction(mlir::Region &function) {
  Block &body = function.front();
  if (body.getNumArguments() != 2) return failure();

  mhlo::ReturnOp return_op = dyn_cast<mhlo::ReturnOp>(body.back());
  if (!return_op) return failure();
  if (return_op.getNumOperands() != 1) return failure();

  ReductionOp reduce_op = dyn_cast_or_null<ReductionOp>(
      return_op.getOperands().front().getDefiningOp());
  if (!reduce_op) return failure();
  if (reduce_op.lhs() != body.getArgument(0) ||
      reduce_op.rhs() != body.getArgument(1))
    return failure();

  return success();
}

// Converts an mhlo.reduce op with the specified BinaryOp as the reduction
// operation into the specified TfOp.
template <typename BinaryOp, typename TfOp>
class ConvertReduceOpToTfOp : public OpConversionPattern<mhlo::ReduceOp> {
 public:
  using OpConversionPattern::OpConversionPattern;

  LogicalResult matchAndRewrite(
      mhlo::ReduceOp reduce_op, ArrayRef<Value> args,
      ConversionPatternRewriter &rewriter) const final {
    if (failed(MatchReduceOpInput(reduce_op))) return failure();

    if (failed(MatchBinaryReduceFunction<BinaryOp>(reduce_op.body())))
      return failure();

    // In `MatchReduceOpInput` function, we already match that the
    // "mhlo::ReduceOp" only has one input, one init_value and one result.
    if (failed(MatchInitValue(reduce_op.init_values()[0]))) return failure();

    auto input = reduce_op.operands()[0];

    // Get reduction dimension.
    DenseIntElementsAttr dimension = reduce_op.dimensions();
    SmallVector<int64_t, 4> reduce_dims;
    for (const int64_t &dim : dimension.getValues<int64_t>()) {
      reduce_dims.emplace_back(dim);
    }
    auto dim_type = RankedTensorType::get(
        {static_cast<int64_t>(reduce_dims.size())}, rewriter.getI64Type());
    auto reduction_indices = rewriter.create<ConstOp>(
        reduce_op.getLoc(), dim_type, rewriter.getI64TensorAttr(reduce_dims));

    rewriter.replaceOpWithNewOp<TfOp>(reduce_op, reduce_op.getType(0), input,
                                      reduction_indices,
                                      /*keep_dim=*/rewriter.getBoolAttr(false));
    return success();
  }

 private:
  // Checks that the init value matches with the init value expected for the
  // target TfOp.
  virtual LogicalResult MatchInitValue(Value init_value) const = 0;

  // This function tries to match that the "mhlo::ReduceOp" only has one
  // input, one init_value and one result.
  LogicalResult MatchReduceOpInput(mhlo::ReduceOp reduce_op) const {
    if (reduce_op.operands().size() != 1 ||
        reduce_op.init_values().size() != 1 ||
        reduce_op.getResults().size() != 1)
      return failure();

    if (!reduce_op.operands()[0].getType().isa<RankedTensorType>())
      return failure();
    if (!reduce_op.getType(0).isa<RankedTensorType>()) return failure();
    return success();
  }
};

class ConvertReduceOpToTfSum
    : public ConvertReduceOpToTfOp<mhlo::AddOp, TF::SumOp> {
 public:
  using ConvertReduceOpToTfOp::ConvertReduceOpToTfOp;

  LogicalResult MatchInitValue(Value init_value) const override {
    DenseFPElementsAttr init_attr;
    if (!matchPattern(init_value, m_Constant(&init_attr)) ||
        !init_attr.isSplat() || !init_attr.getSplatValue<APFloat>().isZero())
      return failure();
    return success();
  }
};

class ConvertReduceOpToTfMax
    : public ConvertReduceOpToTfOp<mhlo::MaxOp, TF::MaxOp> {
 public:
  using ConvertReduceOpToTfOp::ConvertReduceOpToTfOp;

  LogicalResult MatchInitValue(Value init_value) const override {
    DenseFPElementsAttr init_attr;
    if (!matchPattern(init_value, m_Constant(&init_attr)) ||
        !init_attr.isSplat() ||
        !init_attr.getSplatValue<APFloat>().isInfinity() ||
        !init_attr.getSplatValue<APFloat>().isNegative())
      return failure();
    return success();
  }
};

class ConvertReduceOpToTfMin
    : public ConvertReduceOpToTfOp<mhlo::MinOp, TF::MinOp> {
 public:
  using ConvertReduceOpToTfOp::ConvertReduceOpToTfOp;

  LogicalResult MatchInitValue(Value init_value) const override {
    DenseFPElementsAttr init_attr;
    if (!matchPattern(init_value, m_Constant(&init_attr)) ||
        !init_attr.isSplat() ||
        !init_attr.getSplatValue<APFloat>().isInfinity() ||
        init_attr.getSplatValue<APFloat>().isNegative())
      return failure();
    return success();
  }
};

class ConvertIotaOpToTfRange : public OpConversionPattern<mhlo::IotaOp> {
 public:
  using OpConversionPattern::OpConversionPattern;

  LogicalResult matchAndRewrite(
      mhlo::IotaOp iota_op, ArrayRef<Value> args,
      ConversionPatternRewriter &rewriter) const final {
    RankedTensorType type =
        iota_op.getType().dyn_cast_or_null<RankedTensorType>();
    if (!type) return failure();

    const uint64_t dimension = iota_op.iota_dimension();
    Type element_type = type.getElementType();
    Attribute start, limit, delta;
    if (element_type.isa<FloatType>()) {
      start = rewriter.getFloatAttr(element_type, 0.0);
      limit = rewriter.getFloatAttr(element_type, type.getShape()[dimension]);
      delta = rewriter.getFloatAttr(element_type, 1.0);
    } else if (element_type.isa<IntegerType>()) {
      start = rewriter.getIntegerAttr(element_type, 0);
      limit = rewriter.getIntegerAttr(element_type, type.getShape()[dimension]);
      delta = rewriter.getIntegerAttr(element_type, 1);
    } else {
      return failure();
    }

    auto range_type =
        RankedTensorType::get({type.getShape()[dimension]}, element_type);
    Value start_op = rewriter.create<TF::ConstOp>(iota_op.getLoc(), start);
    Value limit_op = rewriter.create<TF::ConstOp>(iota_op.getLoc(), limit);
    Value delta_op = rewriter.create<TF::ConstOp>(iota_op.getLoc(), delta);
    Value result = rewriter.create<TF::RangeOp>(iota_op.getLoc(), range_type,
                                                start_op, limit_op, delta_op);

    if (type.getRank() > 1) {
      std::vector<int64_t> reshape_shape(type.getRank(), 1);
      reshape_shape[iota_op.iota_dimension()] = type.getShape()[dimension];
      auto reshape_type = RankedTensorType::get(reshape_shape, element_type);
      Value reshape_shape_op = rewriter.create<TF::ConstOp>(
          iota_op.getLoc(), rewriter.getI64TensorAttr(reshape_shape));
      result = rewriter.create<TF::ReshapeOp>(iota_op.getLoc(), reshape_type,
                                              result, reshape_shape_op);

      Value broadcast_shape_op = rewriter.create<TF::ConstOp>(
          iota_op.getLoc(), rewriter.getI64TensorAttr(type.getShape()));
      result = rewriter.create<TF::BroadcastToOp>(iota_op.getLoc(), type,
                                                  result, broadcast_shape_op);
    }

    rewriter.replaceOp(iota_op, result);
    return success();
  }
};

// Maps the following represenattions of AvgPool in MHLO into a tf.AvgPool{3D}
// operation when they cleanly map to 2D or 3D average pool with VALID or SAME
// padding:
// * div(reduce_sum_window(x), constant(sizeof(window)))
// * div(reduce_sum_window(x), reduce_sum_window(constant(1)))
class ConvertAvgPoolOp : public OpConversionPattern<mhlo::DivOp> {
 public:
  using OpConversionPattern::OpConversionPattern;

  LogicalResult matchAndRewrite(
      mhlo::DivOp div_op, ArrayRef<Value> args,
      ConversionPatternRewriter &rewriter) const final {
    auto rw =
        dyn_cast_or_null<mhlo::ReduceWindowOp>(div_op.lhs().getDefiningOp());
    if (!rw) return failure();

    // Check that the reduce-window is a sum-reduce-window.
    if (failed(MatchBinaryReduceFunction<mhlo::AddOp>(rw.body())))
      return failure();

    // Check that this is a floating point reduce window with a rank of 4 or 5.
    RankedTensorType rw_type = rw.getType().dyn_cast<RankedTensorType>();
    if (!rw_type || !rw_type.getElementType().isa<FloatType>() ||
        rw_type.getRank() <= 3 || rw_type.getRank() > 5)
      return failure();

    // Check that the Div op doesn't do broadcasting on the output of the reduce
    // window.
    if (div_op.getType() != rw.getType()) return failure();

    // tf.avg_pool need at least 3 dimensions (batch, spatial, channel)
    const uint64_t rank = rw.window_dimensions().size();
    if (rank <= 2) return failure();

    // If the init value isn't zero then it can't be an average pool.
    if (!isFloatZero(rw.init_value())) return failure();

    llvm::SmallVector<int64_t, 5> window_strides;
    if (rw.window_strides().hasValue()) {
      window_strides.insert(window_strides.end(),
                            rw.window_strides()->getValues<int64_t>().begin(),
                            rw.window_strides()->getValues<int64_t>().end());
    } else {
      window_strides.resize(rank, 1);
    }

    llvm::SmallVector<int64_t, 10> padding;
    if (rw.padding().hasValue()) {
      padding.insert(padding.begin(),
                     rw.padding()->getValues<int64_t>().begin(),
                     rw.padding()->getValues<int64_t>().end());
    } else {
      padding.resize(2 * rank, 0);
    }

    // Check that we don't do any reduction along the batch (first) and channel
    // (last) dimensions.
    const uint64_t batch_dim = 0;
    const uint64_t channel_dim = rank - 1;
    if (rw.window_dimensions().getValue<int64_t>({batch_dim}) != 1 ||
        rw.window_dimensions().getValue<int64_t>({channel_dim}) != 1 ||
        window_strides[batch_dim] != 1 || window_strides[channel_dim] != 1 ||
        padding[2 * batch_dim] != 0 || padding[2 * batch_dim + 1] != 0 ||
        padding[2 * channel_dim] != 0 || padding[2 * channel_dim + 1] != 0)
      return failure();

    if (rw.window_dilations().hasValue() &&
        !(rw.window_dilations()->isSplat() &&
          rw.window_dilations()->getSplatValue<APInt>() == 1))
      return failure();

    if (rw.base_dilations().hasValue() &&
        !(rw.base_dilations()->isSplat() &&
          rw.base_dilations()->getSplatValue<APInt>() == 1))
      return failure();

    DenseFPElementsAttr divisor;
    if (matchPattern(div_op.rhs(), m_Constant(&divisor))) {
      // If the divisor is a constant then check that it matches with the number
      // of elements inside the window what is required for a VALID AvgPool.
      if (!divisor.isSplat()) return failure();
      int64_t window_size = 1;
      for (int64_t w : rw.window_dimensions().getValues<int64_t>()) {
        window_size *= w;
      }
      if (!divisor.getSplatValue<APFloat>().isExactlyValue(window_size))
        return failure();

      // Check that we have no padding.
      if (!llvm::all_of(padding, [](int64_t i) { return i == 0; }))
        return failure();

      return replaceWithAvgPool(
          div_op, rw.operand(),
          llvm::to_vector<4>(rw.window_dimensions().getValues<int64_t>()),
          window_strides, "VALID", rewriter);
    }

    auto rw_rhs =
        dyn_cast_or_null<mhlo::ReduceWindowOp>(div_op.rhs().getDefiningOp());
    if (rw_rhs) {
      // Check that RHS is a sum-reduce-window.
      if (failed(MatchBinaryReduceFunction<mhlo::AddOp>(rw_rhs.body())))
        return failure();

      // Check that the RHS is a reduce_window over a constant 1 input with 0 as
      // the init value.
      DenseFPElementsAttr rhs_input;
      if (!isFloatZero(rw_rhs.init_value()) ||
          !matchPattern(rw_rhs.operand(), m_Constant(&rhs_input)) ||
          !rhs_input.isSplat() ||
          !rhs_input.getSplatValue<APFloat>().isExactlyValue(1.0))
        return failure();

      // Check that the two reduce window have the same window configuration.
      if (rw.window_dimensions() != rw_rhs.window_dimensions() ||
          rw.window_strides() != rw_rhs.window_strides() ||
          rw.window_dilations() != rw_rhs.window_dilations() ||
          rw.base_dilations() != rw_rhs.base_dilations() ||
          rw.padding() != rw_rhs.padding())
        return failure();

      if (llvm::all_of(padding, [](int64_t i) { return i == 0; }))
        return replaceWithAvgPool(
            div_op, rw.operand(),
            llvm::to_vector<4>(rw.window_dimensions().getValues<int64_t>()),
            window_strides, "VALID", rewriter);

      RankedTensorType input_type =
          rw.operand().getType().dyn_cast<RankedTensorType>();
      RankedTensorType output_type = rw.getType().dyn_cast<RankedTensorType>();
      if (!input_type || !output_type) return failure();

      // Check that the individual padding values are corresponding to SAME
      // padding from TensorFlow.
      for (uint64_t i = 1; i < rank - 1; ++i) {
        int64_t padding_size =
            (output_type.getShape()[i] - 1) * window_strides[i] +
            rw.window_dimensions().getValue<int64_t>({i}) -
            input_type.getShape()[i];
        if (padding[2 * i] !=
                tensorflow::MathUtil::FloorOfRatio(padding_size, int64_t(2)) ||
            padding[2 * i + 1] !=
                tensorflow::MathUtil::CeilOfRatio(padding_size, int64_t(2)))
          return failure();
      }
      return replaceWithAvgPool(
          div_op, rw.operand(),
          llvm::to_vector<4>(rw.window_dimensions().getValues<int64_t>()),
          window_strides, "SAME", rewriter);
    }
    return failure();
  }

 private:
  bool isFloatZero(Value value) const {
    DenseFPElementsAttr initial_value;
    return matchPattern(value, m_Constant(&initial_value)) &&
           initial_value.getNumElements() == 1 &&
           initial_value.getValue<APFloat>({}).isZero();
  }

  LogicalResult replaceWithAvgPool(mhlo::DivOp op, Value input,
                                   llvm::ArrayRef<int64_t> ksizes,
                                   llvm::ArrayRef<int64_t> kstrides,
                                   llvm::StringRef padding,
                                   ConversionPatternRewriter &rewriter) const {
    if (ksizes.size() == 4) {
      rewriter.replaceOpWithNewOp<AvgPoolOp>(
          op, op.getType(), input, rewriter.getI64ArrayAttr(ksizes),
          rewriter.getI64ArrayAttr(kstrides), rewriter.getStringAttr(padding),
          rewriter.getStringAttr("NHWC"));
      return success();
    } else if (ksizes.size() == 5) {
      rewriter.replaceOpWithNewOp<AvgPool3DOp>(
          op, op.getType(), input, rewriter.getI64ArrayAttr(ksizes),
          rewriter.getI64ArrayAttr(kstrides), rewriter.getStringAttr(padding),
          rewriter.getStringAttr("NDHWC"));
      return success();
    }
    return failure();
  }
};

class LegalizeHloToTf : public PassWrapper<LegalizeHloToTf, FunctionPass> {
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<TF::TensorFlowDialect>();
  }

 public:
  LegalizeHloToTf() = default;
  LegalizeHloToTf(const LegalizeHloToTf &) {}

  /// Performs the legalization to the TF dialect.
  void runOnFunction() override;
};

// Returns the shape of the given value in a Constant Op.
ConstantOp ShapeToConst(PatternRewriter &rewriter, Value value) {
  ArrayRef<int64_t> shape = value.getType().cast<ShapedType>().getShape();
  auto attr_type = RankedTensorType::get({static_cast<int64_t>(shape.size())},
                                         rewriter.getIntegerType(64));
  auto attr = DenseElementsAttr::get(attr_type, shape);
  return rewriter.create<ConstantOp>(value.getLoc(), attr_type, attr);
}

// Converts mhlo.dot to tf.MatMul. Reshape ops will be inserted when
// necessary.
Value ConvertDotOp(PatternRewriter &rewriter, Operation *old_op) {
  auto dot_op = cast<mhlo::DotOp>(old_op);
  const mlir::Location loc = dot_op.getLoc();
  // Normalizes a ShapedType to 2d if the ShapedType is less than 2d by
  // inserting dummy 1-element dimensions in the begining. Does nothing if the
  // old shape is already 2d or higher. This is necessary because tf.MatMul
  // requires input tensors to be at least 2d.
  const auto normalize_rank = [](ShapedType type) -> ShapedType {
    if (type.getRank() >= 2) {
      return type;
    }

    const int rank = type.getRank();
    llvm::SmallVector<int64_t, 2> shape_2d(type.getShape().begin(),
                                           type.getShape().end());
    for (int i = 0; i < 2 - rank; ++i) {
      shape_2d.insert(shape_2d.begin(), 1);
    }
    return RankedTensorType::get(shape_2d, type.getElementType());
  };

  // Reshapes a tensor value to 2d if it is 1d or scalar. Otherwise does
  // nothing.
  const auto reshape_to_2d = [&rewriter, &loc,
                              &normalize_rank](mlir::Value input) {
    const auto input_type = input.getType().cast<ShapedType>();
    if (input_type.getRank() >= 2) {
      return input;
    }

    auto reshape = rewriter.create<mhlo::ReshapeOp>(
        loc, normalize_rank(input_type), input);
    return reshape.getResult();
  };

  // Reshapes both operand to be 2d for tf.MatMul op.
  auto a = reshape_to_2d(dot_op.lhs());
  auto b = reshape_to_2d(dot_op.rhs());
  // Operand `b` needs to be transposed if it is 1d. This is because dot op will
  // contract on the only dimension if rhs is 1d.
  auto b_old_type = dot_op.rhs().getType().cast<ShapedType>();
  BoolAttr transpose_b = rewriter.getBoolAttr(b_old_type.getRank() == 1);
  auto output_type = dot_op.getResult().getType().cast<ShapedType>();
  auto matmul = rewriter.create<TF::MatMulOp>(
      loc, normalize_rank(output_type), a, b,
      /*transpose_a=*/rewriter.getBoolAttr(false), transpose_b);
  auto reshape =
      rewriter.create<mhlo::ReshapeOp>(loc, output_type, matmul.product());
  return reshape.getResult();
}

// Returns true if broadcast_dimensions obey Tensorflow convention, as in new
// dimensions are added as prefix.
bool IsTFStyleBroadcast(DenseIntElementsAttr broadcast_dimensions,
                        Value output) {
  // broadcast_dimensions is an increasing list by definition, thus it suffices
  // to check the first element.
  int64_t input_rank = broadcast_dimensions.getNumElements();
  int64_t output_rank = output.getType().cast<ShapedType>().getRank();
  return input_rank == 0 ||
         (broadcast_dimensions.getValue({0}).cast<IntegerAttr>().getInt() ==
          output_rank - input_rank);
}

// Returns the intermediate shape that input tensor should be reshaped to during
// legalization of BroadcastInDimOp.
ConstantOp ExpandedShape(PatternRewriter &rewriter, Value input,
                         DenseIntElementsAttr broadcast_dimensions,
                         Value output) {
  // Initialize expanded shape with output rank and dimensions of 1.
  SmallVector<Attribute, 4> expanded_shape(
      output.getType().cast<ShapedType>().getRank(),
      /*Value=*/rewriter.getI64IntegerAttr(1));

  // Set dimension sizes specified by broadcast_dimensions.
  ArrayRef<int64_t> input_shape = input.getType().cast<ShapedType>().getShape();
  for (auto x : llvm::enumerate(broadcast_dimensions)) {
    expanded_shape[x.value().getSExtValue()] =
        rewriter.getI64IntegerAttr(input_shape[x.index()]);
  }

  // Create the expanded type wrapped in a ConstantOp.
  auto attr_type =
      RankedTensorType::get({static_cast<int64_t>(expanded_shape.size())},
                            rewriter.getIntegerType(64));
  auto attr = DenseElementsAttr::get(attr_type, expanded_shape);
  return rewriter.create<ConstantOp>(output.getLoc(), attr_type, attr);
}

#include "tensorflow/compiler/mlir/tensorflow/transforms/generated_legalize_hlo.inc"

/// Performs the lowering to XLA dialect.
void LegalizeHloToTf::runOnFunction() {
  MLIRContext &context = getContext();

  // Add legalization patterns to the list.
  OwningRewritePatternList patterns;
  PopulateLegalizeHloToTfPatterns(&patterns, &context);

  ConversionTarget target(context);
  target.addLegalDialect<TensorFlowDialect>();
  target.addLegalOp<CallOp, ConstantOp>();
  if (failed(
          applyPartialConversion(getFunction(), target, std::move(patterns)))) {
    getFunction().emitError("mhlo to TF legalization failed.");
    signalPassFailure();
  }
}

static PassRegistration<LegalizeHloToTf> pass(
    "tf-legalize-hlo", "Legalize from HLO to the TF dialect");

}  // end namespace

void PopulateLegalizeHloToTfPatterns(OwningRewritePatternList *patterns,
                                     MLIRContext *context) {
  patterns->insert<ConvertAvgPoolOp, ConvertConvOp, ConvertSliceOp,
                   ConvertReduceOpToTfMax, ConvertReduceOpToTfMin,
                   ConvertReduceOpToTfSum, ConvertIotaOpToTfRange>(context);
  populateWithGenerated(context, *patterns);
}

std::unique_ptr<OperationPass<FuncOp>> CreateLegalizeHloToTfPass() {
  return std::make_unique<LegalizeHloToTf>();
}

}  // end namespace TF
}  // end namespace mlir
