/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_GPU_GPU_COMPILER_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_GPU_GPU_COMPILER_H_

#include <memory>
#include <string>
#include <vector>

#include "tensorflow/compiler/xla/service/compiler.h"
#include "tensorflow/compiler/xla/service/executable.h"
#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/core/lib/gtl/array_slice.h"
#include "tensorflow/core/platform/macros.h"
#include "tensorflow/core/platform/mutex.h"
#include "tensorflow/core/platform/stream_executor_no_cuda.h"
#include "tensorflow/core/platform/thread_annotations.h"

namespace xla {
namespace gpu {

// The GPU compiler generates efficient GPU executables.
class GpuCompiler : public Compiler {
 public:
  GpuCompiler();
  ~GpuCompiler() override {}

  StatusOr<std::unique_ptr<Executable>> Compile(
      std::unique_ptr<HloModule> module, HloDumper dump_hlo,
      perftools::gputools::StreamExecutor* stream_exec) override;

  StatusOr<std::vector<std::unique_ptr<Executable>>> Compile(
      std::vector<std::unique_ptr<HloModule>> modules, HloDumper dump_hlo,
      std::vector<perftools::gputools::StreamExecutor*> stream_exec) override;

  StatusOr<std::vector<std::unique_ptr<AotCompilationResult>>>
  CompileAheadOfTime(
      std::vector<std::unique_ptr<HloModule>> module,
      HloDumper dump_hlo, AotCompilationOptions const& options) override;

  perftools::gputools::Platform::Id PlatformId() const override;

  HloCostAnalysis::ShapeSizeFunction ShapeSizeBytesFunction() const override {
    // Capture just the pointer size, not the entire GpuCompiler object.
    int64 pointer_size = pointer_size_;
    return [pointer_size](const Shape& shape) {
      return ShapeUtil::ByteSizeOf(shape, pointer_size);
    };
  }

 private:
  // The parent directory of libdevice IR libraries.
  const string libdevice_dir_;

  // The list of PTX strings generated by this GpuCompiler. We let GpuCompiler
  // to own them because they need to be alive across the life span of the
  // StreamExecutor (b/24776264).
  tensorflow::mutex mutex_;
  std::vector<std::unique_ptr<string>> generated_ptxes_ GUARDED_BY(mutex_);

  // The size in bytes of a pointer. Used by ShapeSizeBytesFunction.
  int64 pointer_size_;

  TF_DISALLOW_COPY_AND_ASSIGN(GpuCompiler);
};

}  // namespace gpu
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_GPU_GPU_COMPILER_H_
