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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_GPU_CUSOLVER_REWRITER_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_GPU_CUSOLVER_REWRITER_H_

#include "tensorflow/compiler/xla/service/gpu/cusolver_context.h"
#include "tensorflow/compiler/xla/service/hlo_computation.h"
#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_pass_interface.h"
#include "tensorflow/core/platform/stream_executor_no_cuda.h"
#include "tensorflow/stream_executor/device_memory_allocator.h"

namespace xla {
namespace gpu {

// Rewrites Cholesky calls into CustomCall HLOs that call into cuSolver.
class GpusolverRewriter : public HloModulePass {
 public:
  GpusolverRewriter();
  absl::string_view name() const override { return "gpusolver-rewriter"; }

  using HloPassInterface::Run;
  StatusOr<bool> Run(
      HloModule* module,
      const absl::flat_hash_set<absl::string_view>& execution_threads) override;

 private:
  StatusOr<bool> RunOnComputation(HloComputation* computation);
};

}  // namespace gpu
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_GPU_CUSOLVER_REWRITER_H_
