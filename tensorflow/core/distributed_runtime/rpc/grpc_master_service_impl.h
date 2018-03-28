/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_CORE_DISTRIBUTED_RUNTIME_RPC_GRPC_MASTER_SERVICE_IMPL_H_
#define TENSORFLOW_CORE_DISTRIBUTED_RUNTIME_RPC_GRPC_MASTER_SERVICE_IMPL_H_

#include "grpc++/impl/codegen/async_stream.h"
#include "grpc++/impl/codegen/async_unary_call.h"
#include "grpc++/impl/codegen/proto_utils.h"
#include "grpc++/impl/codegen/rpc_method.h"
#include "grpc++/impl/codegen/service_type.h"
#include "grpc++/impl/codegen/status.h"
#include "grpc++/impl/codegen/stub_options.h"
#include "grpc++/impl/codegen/sync_stream.h"

#include "tensorflow/core/distributed_runtime/rpc/grpc_serialization_traits.h"
#include "tensorflow/core/protobuf/master.pb.h"

// Contains potentially large GraphDef.
TF_GRPC_ALLOW_UNLIMITED_MESSAGE_SIZE(tensorflow::CreateSessionRequest);
// Contains potentially large GraphDef.
TF_GRPC_ALLOW_UNLIMITED_MESSAGE_SIZE(tensorflow::ExtendSessionRequest);
// Contains potentially large TensorProto.
TF_GRPC_ALLOW_UNLIMITED_MESSAGE_SIZE(tensorflow::RunStepRequest);
// Contains potentially large StepStats, TensorProto.
TF_GRPC_ALLOW_UNLIMITED_MESSAGE_SIZE(tensorflow::RunStepResponse);

namespace grpc {
class CompletionQueue;
class Channel;
class RpcService;
class ServerCompletionQueue;
class ServerContext;
}  // namespace grpc

namespace tensorflow {

namespace grpc {

// Implementation of `tensorflow.MasterService`, based on the
// definition in "//tensorflow/core/protobuf/master_service.proto",
// and the gRPC generated stub and service classes.
// See that file for the definition of methods and messages.
class MasterService final {
 public:
  class StubInterface {
   public:
    virtual ~StubInterface() {}
    virtual ::grpc::Status CreateSession(::grpc::ClientContext* context,
                                         const CreateSessionRequest& request,
                                         CreateSessionResponse* response) = 0;
    virtual ::grpc::Status ExtendSession(::grpc::ClientContext* context,
                                         const ExtendSessionRequest& request,
                                         ExtendSessionResponse* response) = 0;
    virtual ::grpc::Status PartialRunSetup(
        ::grpc::ClientContext* context, const PartialRunSetupRequest& request,
        PartialRunSetupResponse* response) = 0;
    virtual ::grpc::Status RunStep(::grpc::ClientContext* context,
                                   const RunStepRequest& request,
                                   RunStepResponse* response) = 0;
    virtual ::grpc::Status CloseSession(::grpc::ClientContext* context,
                                        const CloseSessionRequest& request,
                                        CloseSessionResponse* response) = 0;
    virtual ::grpc::Status ListDevices(::grpc::ClientContext* context,
                                       const ListDevicesRequest& request,
                                       ListDevicesResponse* response) = 0;
    virtual ::grpc::Status Reset(::grpc::ClientContext* context,
                                 const ResetRequest& request,
                                 ResetResponse* response) = 0;
  };
  class Stub final : public StubInterface {
   public:
    Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel);
    ::grpc::Status CreateSession(::grpc::ClientContext* context,
                                 const CreateSessionRequest& request,
                                 CreateSessionResponse* response) override;
    ::grpc::Status ExtendSession(::grpc::ClientContext* context,
                                 const ExtendSessionRequest& request,
                                 ExtendSessionResponse* response) override;
    ::grpc::Status PartialRunSetup(::grpc::ClientContext* context,
                                   const PartialRunSetupRequest& request,
                                   PartialRunSetupResponse* response) override;
    ::grpc::Status RunStep(::grpc::ClientContext* context,
                           const RunStepRequest& request,
                           RunStepResponse* response) override;
    ::grpc::Status CloseSession(::grpc::ClientContext* context,
                                const CloseSessionRequest& request,
                                CloseSessionResponse* response) override;
    ::grpc::Status ListDevices(::grpc::ClientContext* context,
                               const ListDevicesRequest& request,
                               ListDevicesResponse* response) override;
    ::grpc::Status Reset(::grpc::ClientContext* context,
                         const ResetRequest& request,
                         ResetResponse* response) override;

   private:
    std::shared_ptr< ::grpc::ChannelInterface> channel_;
    const ::grpc::internal::RpcMethod rpcmethod_CreateSession_;
    const ::grpc::internal::RpcMethod rpcmethod_ExtendSession_;
    const ::grpc::internal::RpcMethod rpcmethod_PartialRunSetup_;
    const ::grpc::internal::RpcMethod rpcmethod_RunStep_;
    const ::grpc::internal::RpcMethod rpcmethod_CloseSession_;
    const ::grpc::internal::RpcMethod rpcmethod_ListDevices_;
    const ::grpc::internal::RpcMethod rpcmethod_Reset_;
  };
  static std::unique_ptr<Stub> NewStub(
      const std::shared_ptr< ::grpc::ChannelInterface>& channel,
      const ::grpc::StubOptions& options = ::grpc::StubOptions());

  class AsyncService : public ::grpc::Service {
   public:
    AsyncService();
    virtual ~AsyncService();
    void RequestCreateSession(
        ::grpc::ServerContext* context, CreateSessionRequest* request,
        ::grpc::ServerAsyncResponseWriter<CreateSessionResponse>* response,
        ::grpc::CompletionQueue* new_call_cq,
        ::grpc::ServerCompletionQueue* notification_cq, void* tag) {
      ::grpc::Service::RequestAsyncUnary(0, context, request, response,
                                         new_call_cq, notification_cq, tag);
    }
    void RequestExtendSession(
        ::grpc::ServerContext* context, ExtendSessionRequest* request,
        ::grpc::ServerAsyncResponseWriter<ExtendSessionResponse>* response,
        ::grpc::CompletionQueue* new_call_cq,
        ::grpc::ServerCompletionQueue* notification_cq, void* tag) {
      ::grpc::Service::RequestAsyncUnary(1, context, request, response,
                                         new_call_cq, notification_cq, tag);
    }
    void RequestPartialRunSetup(
        ::grpc::ServerContext* context, PartialRunSetupRequest* request,
        ::grpc::ServerAsyncResponseWriter<PartialRunSetupResponse>* response,
        ::grpc::CompletionQueue* new_call_cq,
        ::grpc::ServerCompletionQueue* notification_cq, void* tag) {
      ::grpc::Service::RequestAsyncUnary(2, context, request, response,
                                         new_call_cq, notification_cq, tag);
    }
    void RequestRunStep(
        ::grpc::ServerContext* context, RunStepRequest* request,
        ::grpc::ServerAsyncResponseWriter<RunStepResponse>* response,
        ::grpc::CompletionQueue* new_call_cq,
        ::grpc::ServerCompletionQueue* notification_cq, void* tag) {
      ::grpc::Service::RequestAsyncUnary(3, context, request, response,
                                         new_call_cq, notification_cq, tag);
    }
    void RequestCloseSession(
        ::grpc::ServerContext* context, CloseSessionRequest* request,
        ::grpc::ServerAsyncResponseWriter<CloseSessionResponse>* response,
        ::grpc::CompletionQueue* new_call_cq,
        ::grpc::ServerCompletionQueue* notification_cq, void* tag) {
      ::grpc::Service::RequestAsyncUnary(4, context, request, response,
                                         new_call_cq, notification_cq, tag);
    }
    void RequestListDevices(
        ::grpc::ServerContext* context, ListDevicesRequest* request,
        ::grpc::ServerAsyncResponseWriter<ListDevicesResponse>* response,
        ::grpc::CompletionQueue* new_call_cq,
        ::grpc::ServerCompletionQueue* notification_cq, void* tag) {
      ::grpc::Service::RequestAsyncUnary(5, context, request, response,
                                         new_call_cq, notification_cq, tag);
    }
    void RequestReset(
        ::grpc::ServerContext* context, ResetRequest* request,
        ::grpc::ServerAsyncResponseWriter<ResetResponse>* response,
        ::grpc::CompletionQueue* new_call_cq,
        ::grpc::ServerCompletionQueue* notification_cq, void* tag) {
      ::grpc::Service::RequestAsyncUnary(6, context, request, response,
                                         new_call_cq, notification_cq, tag);
    }
  };
};

}  // namespace grpc

}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_DISTRIBUTED_RUNTIME_RPC_GRPC_MASTER_SERVICE_IMPL_H_
