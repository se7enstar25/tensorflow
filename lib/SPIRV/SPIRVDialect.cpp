//===- LLVMDialect.cpp - MLIR SPIR-V dialect ------------------------------===//
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
//
// This file defines the SPIR-V dialect in MLIR.
//
//===----------------------------------------------------------------------===//

#include "mlir/SPIRV/SPIRVDialect.h"

#include "mlir/IR/MLIRContext.h"
#include "mlir/SPIRV/SPIRVOps.h"

namespace mlir {
namespace spirv {

SPIRVDialect::SPIRVDialect(MLIRContext *context) : Dialect("spv", context) {
  addOperations<
#define GET_OP_LIST
#include "mlir/SPIRV/SPIRVOps.cpp.inc"
      >();
  addOperations<
#define GET_OP_LIST
#include "mlir/SPIRV/SPIRVStructureOps.cpp.inc"
      >();

  // Allow unknown operations because SPIR-V is extensible.
  allowUnknownOperations();
}

} // namespace spirv
} // namespace mlir
