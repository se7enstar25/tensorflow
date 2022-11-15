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

#include "tensorflow/compiler/xla/mlir/xla_cpu/ir/xla_cpu.h"

#include "llvm/ADT/TypeSwitch.h"
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/OpImplementation.h"  // from @llvm-project
#include "tensorflow/compiler/xla/mlir/xla_cpu/ir/xla_cpu_dialect.cc.inc"
#include "tensorflow/compiler/xla/mlir/xla_cpu/ir/xla_cpu_enums.cc.inc"
#define GET_ATTRDEF_CLASSES
#include "tensorflow/compiler/xla/mlir/xla_cpu/ir/xla_cpu_attrdefs.cc.inc"

namespace mlir {
namespace xla_cpu {

void XlaCpuDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "tensorflow/compiler/xla/mlir/xla_cpu/ir/xla_cpu.cc.inc"
#undef GET_OP_LIST
      >();
}

}  // namespace xla_cpu
}  // namespace mlir

#define GET_OP_CLASSES
#include "tensorflow/compiler/xla/mlir/xla_cpu/ir/xla_cpu.cc.inc"
