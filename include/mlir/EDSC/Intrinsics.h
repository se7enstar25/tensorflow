//===- Intrinsics.h - MLIR Operations for Declarative Builders ---*- C++-*-===//
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
// Provides intuitive composable intrinsics for building snippets of MLIR
// declaratively
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_EDSC_INTRINSICS_H_
#define MLIR_EDSC_INTRINSICS_H_

#include "mlir/Support/LLVM.h"

namespace mlir {

namespace edsc {

class ValueHandle;

/// Provides a set of first class intrinsics.
/// In the future, most of intrinsics reated to Instruction that don't contain
/// other instructions should be Tablegen'd.
namespace intrinsics {

// TODO(ntv): Intrinsics below this line should be TableGen'd.
/// Builds an mlir::ReturnOp with the proper `operands` that each must have
/// captured an mlir::Value*.
/// Returns an empty ValueHandle
ValueHandle RETURN(llvm::ArrayRef<ValueHandle> operands);

} // namespace intrinsics

} // namespace edsc

} // namespace mlir

#endif // MLIR_EDSC_INTRINSICS_H_
