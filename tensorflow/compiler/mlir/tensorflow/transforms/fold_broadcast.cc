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

#include <memory>

#include "absl/memory/memory.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Casting.h"
#include "mlir/Dialect/Traits.h"  // from @llvm-project
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project
#include "mlir/IR/Operation.h"  // from @llvm-project
#include "mlir/IR/PatternMatch.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"

namespace mlir {
namespace {

class ConvertResultsBroadcastableShapeOp : public RewritePattern {
 public:
  ConvertResultsBroadcastableShapeOp()
      : RewritePattern(1, MatchAnyOpTypeTag()) {}

  LogicalResult matchAndRewrite(Operation* op,
                                PatternRewriter& rewriter) const override;

 private:
  template <typename Op>
  LogicalResult RewriteEqOp(Operation* op, PatternRewriter& rewriter) const;

  LogicalResult RewriteOp(Operation* op, PatternRewriter& rewriter) const;
};

class BroadcastFoldPass : public PassWrapper<BroadcastFoldPass, FunctionPass> {
 public:
  void runOnFunction() override;
};

LogicalResult ConvertResultsBroadcastableShapeOp::matchAndRewrite(
    Operation* op, PatternRewriter& rewriter) const {
  if (op->hasTrait<OpTrait::ResultsBroadcastableShape>())
    return RewriteOp(op, rewriter);

  // tf.Equal and tf.NotEqual ops only satisfy ResultsBroadcastableShape when
  // incompatible_shape_error is `true` (what is also checked by the verifier).
  if (succeeded(RewriteEqOp<TF::EqualOp>(op, rewriter))) return success();
  if (succeeded(RewriteEqOp<TF::NotEqualOp>(op, rewriter))) return success();

  return failure();
}

template <typename Op>
LogicalResult ConvertResultsBroadcastableShapeOp::RewriteEqOp(
    Operation* op, PatternRewriter& rewriter) const {
  auto eq_op = llvm::dyn_cast_or_null<Op>(op);
  if (eq_op && eq_op.incompatible_shape_error()) return RewriteOp(op, rewriter);
  return failure();
}

LogicalResult ConvertResultsBroadcastableShapeOp::RewriteOp(
    Operation* op, PatternRewriter& rewriter) const {
  if (op->getNumOperands() != 2 || op->getResultTypes().size() != 1)
    return failure();

  // Check that the result shape is fully defined.
  auto result_type =
      op->getResultTypes().front().dyn_cast_or_null<RankedTensorType>();
  if (!result_type || !result_type.hasStaticShape()) return failure();

  bool changed = false;
  for (uint64_t i = 0, e = op->getNumOperands(); i < e; ++i) {
    // Check that the i'th operand is a broadcast.
    auto broadcast = llvm::dyn_cast_or_null<TF::BroadcastToOp>(
        op->getOpOperand(i).get().getDefiningOp());
    if (!broadcast) continue;

    // Check that the operand of the broadcast has fully defined shape.
    auto broadcast_arg_type =
        broadcast.input().getType().dyn_cast_or_null<RankedTensorType>();
    if (!broadcast_arg_type || !broadcast_arg_type.hasStaticShape()) continue;

    // Check that the other argument has fully defined shape.
    auto argument_type = op->getOpOperand(1 - i)
                             .get()
                             .getType()
                             .dyn_cast_or_null<RankedTensorType>();
    if (!argument_type || !argument_type.hasStaticShape()) continue;

    // Check that the input of the broadcast and the other operand is broadcast
    // compatible.
    llvm::SmallVector<int64_t, 4> broadcasted_shape;
    if (!OpTrait::util::getBroadcastedShape(broadcast_arg_type.getShape(),
                                            argument_type.getShape(),
                                            broadcasted_shape))
      continue;

    // Check that an implicit broadcast between the operand of the broadcast and
    // the other argument would result in the same type as the result type.
    if (broadcasted_shape != result_type.getShape()) continue;

    // Update the operand of the op to be the operand of the broadcast.
    rewriter.updateRootInPlace(
        op, [&]() { op->getOpOperand(i).set(broadcast.input()); });
    changed = true;
  }
  return success(changed);
}

void BroadcastFoldPass::runOnFunction() {
  OwningRewritePatternList patterns;
  auto func = getFunction();

  patterns.insert<ConvertResultsBroadcastableShapeOp>();
  applyPatternsAndFoldGreedily(func, std::move(patterns));
}

}  // namespace

namespace TF {
std::unique_ptr<OperationPass<FuncOp>> CreateBroadcastFoldPass() {
  return absl::make_unique<BroadcastFoldPass>();
}
}  // namespace TF

static PassRegistration<BroadcastFoldPass> pass(
    "tf-broadcast-fold",
    "Fold explicit broadcasts into the following operations if they support "
    "implicit broadcasting on their operand.");

}  // namespace mlir
