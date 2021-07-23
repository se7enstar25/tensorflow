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

#ifndef TENSORFLOW_COMPILER_MLIR_TOOLS_KERNEL_GEN_TESTS_TF_FRAMEWORK_C_INTERFACE_H_
#define TENSORFLOW_COMPILER_MLIR_TOOLS_KERNEL_GEN_TESTS_TF_FRAMEWORK_C_INTERFACE_H_

#include "mlir/ExecutionEngine/RunnerUtils.h"  // from @llvm-project

namespace mlir {
namespace kernel_gen {
namespace tf_framework {

extern "C" MLIR_RUNNERUTILS_EXPORT void _mlir_ciface_tf_launch_kernel(
    void* ctx, void* module_blob, char* kernel_name, intptr_t gridX,
    intptr_t gridY, intptr_t gridZ, intptr_t blockX, intptr_t blockY,
    intptr_t blockZ, void** params);

extern "C" MLIR_RUNNERUTILS_EXPORT void* _mlir_ciface_tf_alloc(
    void* op_kernel_ctx, size_t num_elements, size_t element_size,
    int32_t output_index, int32_t num_candidates,
    int32_t* candidate_input_indices);

extern "C" MLIR_RUNNERUTILS_EXPORT void _mlir_ciface_tf_dealloc(
    void* op_kernel_ctx, void* ptr);

extern "C" MLIR_RUNNERUTILS_EXPORT void _mlir_ciface_tf_report_error(
    void* op_kernel_ctx, int32_t error_code, char* msg);

extern "C" MLIR_RUNNERUTILS_EXPORT void* _mlir_ciface_tf_jit_compile(
    void* op_kernel_ctx, char* code);

extern "C" MLIR_RUNNERUTILS_EXPORT void _mlir_ciface_tf_jit_execute(
    void* op_kernel_ctx, void* callable, void* result, int64_t arg_rank,
    void* arg_descr);

}  // namespace tf_framework
}  // namespace kernel_gen
}  // namespace mlir

#endif  // TENSORFLOW_COMPILER_MLIR_TOOLS_KERNEL_GEN_TESTS_TF_FRAMEWORK_C_INTERFACE_H_
