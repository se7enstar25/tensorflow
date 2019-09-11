//===- ConvertSimQuant.cpp - Converts simulated quant ops------------------===//
//
// Copyright 2019 The MLIR Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================

#include "mlir/Dialect/QuantOps/FakeQuantSupport.h"
#include "mlir/Dialect/QuantOps/Passes.h"
#include "mlir/Dialect/QuantOps/QuantOps.h"
#include "mlir/Dialect/QuantOps/UniformSupport.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/StandardTypes.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::quant;

namespace {

class ConvertSimulatedQuantPass
    : public FunctionPass<ConvertSimulatedQuantPass> {
public:
  void runOnFunction() override;
};

} // end anonymous namespace

/// Base class rewrites ConstFakeQuant into a qbarrier/dbarrier pair.
template <typename ConcretRewriteClass, typename FakeQuantOp>
class FakeQuantRewrite : public OpRewritePattern<FakeQuantOp> {
public:
  using OpRewritePattern<FakeQuantOp>::OpRewritePattern;

  FakeQuantRewrite(MLIRContext *ctx, bool *hadFailure)
      : OpRewritePattern<FakeQuantOp>(ctx), hadFailure(hadFailure) {}

  PatternMatchResult matchAndRewrite(FakeQuantOp op,
                                     PatternRewriter &rewriter) const override {
    // TODO: If this pattern comes up more frequently, consider adding core
    // support for failable rewrites.
    if (failableRewrite(op, rewriter)) {
      *hadFailure = true;
      return Pattern::matchFailure();
    }

    return Pattern::matchSuccess();
  }

private:
  bool *hadFailure;

  bool failableRewrite(FakeQuantOp op, PatternRewriter &rewriter) const {
    auto converter = ExpressedToQuantizedConverter::forInputType(op.getType());
    if (!converter) {
      return (op.emitError("unsupported quantized type conversion"), true);
    }

    QuantizedType elementType =
        static_cast<const ConcretRewriteClass *>(this)
            ->convertFakeQuantAttrsToType(op, converter.expressedType);

    if (!elementType) {
      // Note that the fakeQuantAttrsToType will have emitted the error.
      return true;
    }

    Type quantizedType = converter.convert(elementType);
    assert(quantizedType &&
           "Converter accepted a type that it did not convert");

    // TODO: Map to a qbarrier with an attribute like [Forced] to signal that
    // this is a forced/hard-coded constraint.
    auto qbarrier = rewriter.create<QuantizeCastOp>(op.getLoc(), quantizedType,
                                                    op.inputs());
    rewriter.replaceOpWithNewOp<DequantizeCastOp>(op, converter.inputType,
                                                  qbarrier.getResult());

    return false;
  }
};

class ConstFakeQuantRewrite
    : public FakeQuantRewrite<ConstFakeQuantRewrite, ConstFakeQuant> {
public:
  using BaseRewrite = FakeQuantRewrite<ConstFakeQuantRewrite, ConstFakeQuant>;

  ConstFakeQuantRewrite(MLIRContext *ctx, bool *hadFailure)
      : BaseRewrite(ctx, hadFailure) {}

  QuantizedType convertFakeQuantAttrsToType(ConstFakeQuant fqOp,
                                            Type expressedType) const {
    return fakeQuantAttrsToType(
        fqOp.getLoc(), fqOp.num_bits().getSExtValue(),
        fqOp.min().convertToFloat(), fqOp.max().convertToFloat(),
        fqOp.narrow_range(), expressedType, fqOp.is_signed());
  }
};

class ConstFakeQuantPerAxisRewrite
    : public FakeQuantRewrite<ConstFakeQuantPerAxisRewrite,
                              ConstFakeQuantPerAxis> {
public:
  using BaseRewrite =
      FakeQuantRewrite<ConstFakeQuantPerAxisRewrite, ConstFakeQuantPerAxis>;

  ConstFakeQuantPerAxisRewrite(MLIRContext *ctx, bool *hadFailure)
      : BaseRewrite(ctx, hadFailure) {}

  QuantizedType convertFakeQuantAttrsToType(ConstFakeQuantPerAxis fqOp,
                                            Type expressedType) const {
    SmallVector<double, 4> min, max;
    min.reserve(fqOp.min().size());
    max.reserve(fqOp.max().size());
    for (auto m : fqOp.min())
      min.push_back(m.cast<FloatAttr>().getValueAsDouble());
    for (auto m : fqOp.max())
      max.push_back(m.cast<FloatAttr>().getValueAsDouble());

    return fakeQuantAttrsToType(fqOp.getLoc(), fqOp.num_bits().getSExtValue(),
                                fqOp.axis().getSExtValue(), min, max,
                                fqOp.narrow_range(), expressedType,
                                fqOp.is_signed());
  }
};

void ConvertSimulatedQuantPass::runOnFunction() {
  bool hadFailure = false;
  OwningRewritePatternList patterns;
  auto func = getFunction();
  auto ctx = func.getContext();
  patterns.insert<ConstFakeQuantRewrite, ConstFakeQuantPerAxisRewrite>(
      ctx, &hadFailure);
  applyPatternsGreedily(func, patterns);
  if (hadFailure)
    signalPassFailure();
}

std::unique_ptr<FunctionPassBase>
mlir::quant::createConvertSimulatedQuantPass() {
  return std::make_unique<ConvertSimulatedQuantPass>();
}

static PassRegistration<ConvertSimulatedQuantPass>
    pass("quant-convert-simulated-quantization",
         "Converts training-time simulated quantization ops to corresponding "
         "quantize/dequantize casts.");
