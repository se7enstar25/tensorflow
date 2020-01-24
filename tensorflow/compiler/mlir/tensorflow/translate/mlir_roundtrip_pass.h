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

#ifndef TENSORFLOW_COMPILER_MLIR_TENSORFLOW_TRANSLATE_MLIR_ROUNDTRIP_PASS_H_
#define TENSORFLOW_COMPILER_MLIR_TENSORFLOW_TRANSLATE_MLIR_ROUNDTRIP_PASS_H_

#include "mlir/Dialect/StandardOps/Ops.h"  // TF:llvm-project
#include "tensorflow/core/common_runtime/optimization_registry.h"
#include "tensorflow/core/lib/core/status.h"

namespace tensorflow {

// An optimization pass that simply roundtrips the Graph to MLIR and back.
class MlirRoundtripPass : public GraphOptimizationPass {
 public:
  Status Run(const GraphOptimizationPassOptions& options) override;
};

// An optimization pass that simply imports the Graph to MLIR.
class MlirImportPass : public GraphOptimizationPass {
 public:
  Status Run(const GraphOptimizationPassOptions& options) override;
};

}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_MLIR_TENSORFLOW_TRANSLATE_MLIR_ROUNDTRIP_PASS_H_
