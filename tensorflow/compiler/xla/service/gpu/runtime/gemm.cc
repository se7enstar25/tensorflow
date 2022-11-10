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

#include "tensorflow/compiler/xla/service/gpu/runtime/gemm.h"

#include <utility>

#include "tensorflow/compiler/xla/runtime/custom_call.h"
#include "tensorflow/compiler/xla/runtime/executable.h"
#include "tensorflow/compiler/xla/service/gpu/matmul_utils.h"
#include "tensorflow/compiler/xla/service/gpu/runtime/support.h"
#include "tensorflow/compiler/xla/service/service_executable_run_options.h"
#include "tensorflow/compiler/xla/xla.pb.h"

namespace xla {
namespace gpu {

using xla::runtime::CustomCall;
using xla::runtime::Executable;

const GemmConfig* GemmConfigCache::Get(int64_t uid) {
  absl::MutexLock lock(&mutex_);
  auto it = configs_.find(uid);
  if (it != configs_.end()) return &it->second;
  return nullptr;
}

const GemmConfig* GemmConfigCache::Set(int64_t uid, GemmConfig config) {
  absl::MutexLock lock(&mutex_);
  auto it = configs_.find(uid);
  if (it != configs_.end()) return &it->second;

  auto emplaced = configs_.try_emplace(uid, std::move(config));
  return &emplaced.first->second;
}

// -------------------------------------------------------------------------- //

namespace {
struct Gemm {
  LLVM_ATTRIBUTE_ALWAYS_INLINE
  absl::Status operator()(const ServiceExecutableRunOptions* run_options,
                          const DebugOptions* debug_options,
                          GemmConfigCache* configs,
                          runtime::StridedMemrefView lhs,
                          runtime::StridedMemrefView rhs,
                          runtime::StridedMemrefView out, int64_t algorithm,
                          double alpha_real, double alpha_imag, double beta,
                          DotDimensionNumbers dot_dims, int64_t uid) const;

  static Gemm Handler() { return Gemm(); }
};
}  // namespace

absl::Status Gemm::operator()(const ServiceExecutableRunOptions* run_options,
                              const DebugOptions* debug_options,
                              GemmConfigCache* configs,
                              runtime::StridedMemrefView lhs,
                              runtime::StridedMemrefView rhs,
                              runtime::StridedMemrefView out, int64_t algorithm,
                              double alpha_real, double alpha_imag, double beta,
                              DotDimensionNumbers dot_dims, int64_t uid) const {
  se::DeviceMemoryBase lhs_data = GetDeviceAddress(lhs);
  se::DeviceMemoryBase rhs_data = GetDeviceAddress(rhs);
  se::DeviceMemoryBase output_data = GetDeviceAddress(out);

  VLOG(3) << "Running GEMM";
  se::Stream* stream = run_options->stream();

  // Find the gemm config for this instance of operation based on uid.
  const GemmConfig* config = configs->Get(uid);
  if (config == nullptr) {
    auto cfg = GetGemmConfig(lhs, rhs, out, algorithm, alpha_real, alpha_imag,
                             beta, dot_dims.lhs_batch, dot_dims.lhs_contract,
                             dot_dims.rhs_batch, dot_dims.rhs_contract);
    if (!cfg.ok()) return ToAbslStatus(cfg.status());
    config = configs->Set(uid, std::move(*cfg));
  }

  Status executed = [&]() -> Status {
    return RunGemm(*config, lhs_data, rhs_data, output_data, stream);
  }();

  if (!executed.ok()) return ToAbslStatus(executed);

  return absl::OkStatus();
}

static bool Gemm(runtime::ExecutionContext* ctx, void** args, void** attrs,
                 void** rets) {
  static auto* handler = CustomCall::Bind("xla.gpu.gemm")
                             .UserData<const ServiceExecutableRunOptions*>()
                             .UserData<const DebugOptions*>()
                             .UserData<GemmConfigCache*>()
                             .Arg<runtime::StridedMemrefView>()  // lhs
                             .Arg<runtime::StridedMemrefView>()  // rhs
                             .Arg<runtime::StridedMemrefView>()  // out
                             .Attr<int64_t>("algorithm")
                             .Attr<double>("alpha_real")
                             .Attr<double>("alpha_imag")
                             .Attr<double>("beta")
                             .Attr<DotDimensionNumbers>("dot_dims")
                             .Attr<int64_t>("uid")
                             .To<checks>(Gemm::Handler())
                             .release();

  return succeeded(Executable::Call(ctx, *handler, args, attrs, rets));
}

void RegisterGemmCustomCalls(runtime::DirectCustomCallRegistry& registry) {
  registry.Register("xla.gpu.gemm", &xla::gpu::Gemm);
}

}  // namespace gpu
}  // namespace xla
