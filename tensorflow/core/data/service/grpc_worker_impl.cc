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

#include "tensorflow/core/data/service/grpc_worker_impl.h"

#include "grpcpp/server_context.h"
#include "tensorflow/core/distributed_runtime/rpc/grpc_util.h"

namespace tensorflow {
namespace data {

using ::grpc::ServerBuilder;
using ::grpc::ServerContext;
using ::grpc::Status;

GrpcWorkerImpl::GrpcWorkerImpl(ServerBuilder* server_builder,
                               const experimental::WorkerConfig& config)
    : impl_(config) {
  server_builder->RegisterService(this);
  VLOG(1) << "Registered data service worker";
}

void GrpcWorkerImpl::Start(const std::string& worker_address) {
  impl_.Start(worker_address);
}

#define HANDLER(method)                                         \
  Status GrpcWorkerImpl::method(ServerContext* context,         \
                                const method##Request* request, \
                                method##Response* response) {   \
    return ToGrpcStatus(impl_.method(request, response));       \
  }
HANDLER(ProcessTask);
HANDLER(GetElement);
#undef HANDLER

}  // namespace data
}  // namespace tensorflow
