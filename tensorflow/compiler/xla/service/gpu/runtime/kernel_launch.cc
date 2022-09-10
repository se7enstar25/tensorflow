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

#include "tensorflow/compiler/xla/service/gpu/runtime/kernel_launch.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "tensorflow/compiler/xla/runtime/custom_call.h"
#include "tensorflow/compiler/xla/runtime/executable.h"
#include "tensorflow/compiler/xla/service/gpu/launch_dimensions.h"
#include "tensorflow/compiler/xla/service/gpu/runtime/support.h"
#include "tensorflow/compiler/xla/service/gpu/stream_executor_util.h"
#include "tensorflow/compiler/xla/service/service_executable_run_options.h"

namespace xla {
namespace gpu {

using xla::runtime::CustomCall;
using xla::runtime::Executable;
using xla::runtime::FlatMemrefView;
using xla::runtime::StridedMemrefView;

//===----------------------------------------------------------------------===//
// Keep track of all device kernels loaded by a XLA runtime Gpu executable.
//===----------------------------------------------------------------------===//

se::KernelBase* GpuExecutableKernelsCache::Get(se::StreamExecutor* executor,
                                               std::string_view name) {
  Key key(executor, name);

  absl::MutexLock lock(&mutex_);
  auto it = kernels_cache_.find(key);
  if (it != kernels_cache_.end()) return it->second.get();

  return nullptr;
}

se::KernelBase* GpuExecutableKernelsCache::Set(
    se::StreamExecutor* executor, std::string_view name,
    std::unique_ptr<se::KernelBase> kernel) {
  Key key(executor, name);

  absl::MutexLock lock(&mutex_);
  auto it = kernels_cache_.find(key);
  if (it != kernels_cache_.end()) return it->second.get();

  auto emplaced = kernels_cache_.try_emplace(key, std::move(kernel));
  return emplaced.first->second.get();
}

//===----------------------------------------------------------------------===//
// Define the kernel launch custom call.
//===----------------------------------------------------------------------===//

static absl::Status LaunchFunc(
    const ServiceExecutableRunOptions* run_options, const std::string* ptx,
    const std::vector<uint8_t>* cubin, se::DeviceMemoryBase* temp_buffer,
    GpuExecutableKernelsCache* kernels_cache, int32_t grid_size_x,
    int32_t grid_size_y, int32_t grid_size_z, int32_t block_size_x,
    int32_t block_size_y, int32_t block_size_z, CustomCall::RemainingArgs args,
    std::string_view name) {
  se::Stream* stream = run_options->stream();
  se::StreamExecutor* executor = stream->parent();

  LaunchDimensions launch_dimensions(
      {grid_size_x, grid_size_y, grid_size_z},
      {block_size_x, block_size_y, block_size_z});

  se::KernelBase* kernel = kernels_cache->Get(executor, name);
  const int args_size_including_temp_buffer = args.size() + 1;

  // If kernel does not exists create it from the ptx and dubin.
  if (kernel == nullptr) {
    auto created =
        CreateKernel(absl::string_view(name.data(), name.size()),
                     args_size_including_temp_buffer, *ptx, *cubin, executor);
    if (!created.ok()) return ToAbslStatus(created.status());

    kernel = kernels_cache->Set(executor, name, std::move(*created));
  }

  VLOG(3) << "Launching " << kernel->name();
  absl::InlinedVector<se::DeviceMemoryBase, 4> buffer_args;
  buffer_args.reserve(args_size_including_temp_buffer);

  // Add MemRef arguments as buffer arguments.
  for (unsigned i = 0; i < args.size(); ++i) {
    // Simple row major memref passed as shapeless buffer.
    if (auto memref = args.get<FlatMemrefView>(i); succeeded(memref)) {
      buffer_args.emplace_back(GetDeviceAddress(*memref));
      continue;
    }

    // Memref layout must be encoded in the compiled device kernel, so we don't
    // have to pass strides or minor to major dimensions order to the kernel.
    if (auto strided = args.get<StridedMemrefView>(i); succeeded(strided)) {
      buffer_args.emplace_back(GetDeviceAddress(*strided));
      continue;
    }

    return absl::InvalidArgumentError(
        absl::StrFormat("Unsupported argument #%d type", i));
  }

  // Always add temporary buffer as the last kernel argument.
  buffer_args.push_back(*temp_buffer);

  // Execute device kernel on a main stream.
  auto executed =
      ExecuteKernelOnStream(*kernel, buffer_args, launch_dimensions, stream);
  if (!executed.ok()) return ToAbslStatus(executed);

  return absl::OkStatus();
}

//===----------------------------------------------------------------------===//

static bool Launch(runtime::ExecutionContext* ctx, void** args, void** attrs,
                   void** rets) {
  static auto* handler = CustomCall::Bind("xla.gpu.func.launch")
                             .UserData<const ServiceExecutableRunOptions*>()
                             .UserData<const std::string*>()
                             .UserData<const std::vector<uint8_t>*>()
                             .UserData<se::DeviceMemoryBase*>()
                             .UserData<GpuExecutableKernelsCache*>()
                             .Arg<int32_t>()   // grid_size_x
                             .Arg<int32_t>()   // grid_size_y
                             .Arg<int32_t>()   // grid_size_z
                             .Arg<int32_t>()   // block_size_x
                             .Arg<int32_t>()   // block_size_y
                             .Arg<int32_t>()   // block_size_x
                             .RemainingArgs()  // args
                             .Attr<std::string_view>("kernel")
                             .To<checks>(LaunchFunc)
                             .release();

  return succeeded(Executable::Call(ctx, *handler, args, attrs, rets));
}

void RegisterKernelLaunchCustomCalls(
    runtime::DirectCustomCallRegistry& registry) {
  registry.Register("xla.gpu.func.launch", Launch);
}

}  // namespace gpu
}  // namespace xla
