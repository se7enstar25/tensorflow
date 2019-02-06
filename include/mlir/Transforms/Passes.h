//===- Passes.h - Pass Entrypoints ------------------------------*- C++ -*-===//
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
// This header file defines prototypes that expose pass constructors in the loop
// transformation library.
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_TRANSFORMS_PASSES_H
#define MLIR_TRANSFORMS_PASSES_H

#include "mlir/Support/LLVM.h"
#include <limits>

namespace mlir {

class AffineForOp;
template <typename T> class ConstOpPointer;
class FunctionPass;
class ModulePass;

/// Creates a constant folding pass.
FunctionPass *createConstantFoldPass();

/// Creates an instance of the Canonicalizer pass.
FunctionPass *createCanonicalizerPass();

/// Creates a pass to perform common sub expression elimination.
FunctionPass *createCSEPass();

/// Creates a pass to vectorize loops, operations and data types using a
/// target-independent, n-D super-vector abstraction.
FunctionPass *createVectorizePass();

/// Creates a pass to allow independent testing of vectorizer functionality with
/// FileCheck.
FunctionPass *createVectorizerTestPass();

/// Creates a pass to lower super-vectors to target-dependent HW vectors.
FunctionPass *createMaterializeVectorsPass();

/// Creates a loop unrolling pass with the provided parameters.
/// 'getUnrollFactor' is a function callback for clients to supply a function
/// that computes an unroll factor - the callback takes precedence over unroll
/// factors supplied through other means. If -1 is passed as the unrollFactor
/// and no callback is provided, anything passed from the command-line (if at
/// all) or the default unroll factor is used (LoopUnroll:kDefaultUnrollFactor).
FunctionPass *
createLoopUnrollPass(int unrollFactor = -1, int unrollFull = -1,
                     const std::function<unsigned(ConstOpPointer<AffineForOp>)>
                         &getUnrollFactor = nullptr);

/// Creates a loop unroll jam pass to unroll jam by the specified factor. A
/// factor of -1 lets the pass use the default factor or the one on the command
/// line if provided.
FunctionPass *createLoopUnrollAndJamPass(int unrollJamFactor = -1);

/// Creates an simplification pass for affine structures.
FunctionPass *createSimplifyAffineStructuresPass();

/// Creates a loop fusion pass which fuses loops.
FunctionPass *createLoopFusionPass();

/// Creates a pass to pipeline explicit movement of data across levels of the
/// memory hierarchy.
FunctionPass *createPipelineDataTransferPass();

/// Lowers affine control flow instructions (ForStmt, IfStmt and AffineApplyOp)
/// to equivalent lower-level constructs (flow of basic blocks and arithmetic
/// primitives).
FunctionPass *createLowerAffinePass();

/// Creates a pass to perform tiling on loop nests.
FunctionPass *createLoopTilingPass();

/// Promotes all accessed memref regions to the specified faster memory space
/// while generating DMAs to move data.
FunctionPass *createDmaGenerationPass(
    unsigned slowMemorySpace, unsigned fastMemorySpace,
    int minDmaTransferSize = 1024,
    uint64_t fastMemCapacityBytes = std::numeric_limits<uint64_t>::max());

/// Creates a pass to lower VectorTransferReadOp and VectorTransferWriteOp.
FunctionPass *createLowerVectorTransfersPass();

/// Creates a pass to perform optimizations relying on memref dataflow such as
/// store to load forwarding, elimination of dead stores, and dead allocs.
FunctionPass *createMemRefDataFlowOptPass();

/// Creates a pass to strip debug information from a function.
FunctionPass *createStripDebugInfoPass();

/// Creates a pass to convert Standard and Builtin dialects into the LLVMIR
/// dialect.
ModulePass *createConvertToLLVMIRPass();

} // end namespace mlir

#endif // MLIR_TRANSFORMS_PASSES_H
