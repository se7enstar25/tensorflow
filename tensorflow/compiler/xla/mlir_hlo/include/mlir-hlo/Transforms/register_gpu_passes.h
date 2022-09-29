/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#ifndef MLIR_HLO_TRANSFORMS_REGISTER_GPU_PASSES_H
#define MLIR_HLO_TRANSFORMS_REGISTER_GPU_PASSES_H

#include "mlir-hlo/Transforms/gpu_passes.h"
#include "mlir/Pass/Pass.h"

namespace mlir {
namespace hlo {

#define GEN_PASS_REGISTRATION
#include "mlir-hlo/Transforms/gpu_passes.h.inc"

namespace impl {
struct HloToGpuPipelineOptions
    : public PassPipelineOptions<HloToGpuPipelineOptions> {
  ListOption<int64_t> blockTileDim{
      *this, "block-tile",
      llvm::cl::desc("dimensions of the subproblem processed by the block")};
  ListOption<int64_t> warpTileDim{
      *this, "warp-tile",
      llvm::cl::desc("dimensions of the subproblem processed by the warp")};
  ListOption<int64_t> threadTileDim{
      *this, "thread-tile",
      llvm::cl::desc("dimensions of the subproblem processed by the thread")};
};
}  // namespace impl

/// Registers all passes defined in gpu_passes.td.
inline void registerAllHloGpuPasses() {
  registerLMHLOGPUTransformsPasses();
  PassPipelineRegistration<impl::HloToGpuPipelineOptions>(
      "hlo-to-gpu-pipeline",
      "Pipeline to transform HLO to LLVM + NVVM dialects.",
      [](OpPassManager &pm, const impl::HloToGpuPipelineOptions &options) {
        createHloToGpuPipeline(pm, options.blockTileDim, options.warpTileDim,
                               options.threadTileDim);
      });
}

}  // end namespace hlo
}  // end namespace mlir

#endif  // MLIR_HLO_TRANSFORMS_REGISTER_GPU_PASSES_H
