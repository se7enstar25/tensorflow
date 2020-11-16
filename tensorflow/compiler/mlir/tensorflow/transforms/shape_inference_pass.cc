/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

#include "mlir/IR/StandardTypes.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/transforms/shape_inference.h"
#include "tensorflow/core/framework/shape_inference.h"

#define DEBUG_TYPE "tf-shape-inference"

namespace mlir {
namespace TF {

namespace {

// This transformation pass propagate shapes on the TensorFlow graph.
// It is a ModulePass in order to be able to change function types.
class ShapeInference
    : public PassWrapper<ShapeInference, OperationPass<ModuleOp>> {
 public:
  ShapeInference() = default;
  void runOnOperation() override {
    if (failed(InferModuleShape(getOperation()))) return signalPassFailure();
  }
};

PassRegistration<ShapeInference> pass(
    "tf-shape-inference", "Simple Shape Inference on TensorFlow Dialect");

}  // namespace

std::unique_ptr<OperationPass<ModuleOp>> CreateTFShapeInferencePass() {
  return std::make_unique<ShapeInference>();
}

}  // namespace TF
}  // namespace mlir
