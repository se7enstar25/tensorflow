/* Copyright 2015 Google Inc. All Rights Reserved.

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

#include "tensorflow/core/common_runtime/gpu/gpu_util.h"

#include "tensorflow/core/common_runtime/copy_tensor.h"
#include "tensorflow/core/common_runtime/device.h"
#include "tensorflow/core/common_runtime/dma_helper.h"
#include "tensorflow/core/common_runtime/gpu/gpu_event_mgr.h"
#include "tensorflow/core/common_runtime/gpu/process_state.h"
#include "tensorflow/core/common_runtime/gpu_device_context.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_reference.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/core/refcount.h"
#include "tensorflow/core/lib/gtl/array_slice.h"
#include "tensorflow/core/lib/gtl/stl_util.h"
#include "tensorflow/core/lib/hash/hash.h"
#include "tensorflow/core/lib/strings/strcat.h"
#include "tensorflow/core/lib/strings/stringprintf.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/stream_executor.h"
#include "tensorflow/core/platform/tensor_coding.h"
#include "tensorflow/core/platform/tracing.h"
#include "tensorflow/core/util/util.h"

// IMPLEMENTATION NOTE:
//
// 1. Within this module, we intentionally LOG(FATAL) if any stream
//    involved in memcpy becomes !stream->ok(), because TF process
//    today (1/2016) can not properly recover from such an error.
//
// 2. When 0-size tensor is being copied, we should not schedule a
//    copy ThenMemcpy since there is no byte to move. However, we must
//    ensure the causal ordering by arranging the copy done callback
//    happens-after all activities scheduled on the given stream being
//    finished.

// If this need to be runtime configurable, consider adding options to
// ConfigProto.
const tensorflow::int64 FLAGS_brain_gpu_util_debug_string_maxlen = 128;
extern bool FLAGS_brain_gpu_record_mem_types;

using perftools::gputools::DeviceMemoryBase;
using perftools::gputools::DeviceMemory;
using perftools::gputools::Stream;

namespace tensorflow {

namespace gpu = ::perftools::gputools;

Status PrepareCopy(Device* device, const DeviceContext* ctx, const Tensor& src,
                   const Tensor* dst,
                   const DeviceBase::GpuDeviceInfo** dev_info,
                   gpu::Stream** stream) {
  if (device == nullptr) {
    return errors::Internal("Unexpected null device.");
  }
  auto di = device->tensorflow_gpu_device_info();
  if (di == nullptr) {
    return errors::Internal("Unexpected null device info.");
  }
  *dev_info = di;
  if (ctx == nullptr) {
    return errors::Internal("Unexpected null device context.");
  }
  auto gs = static_cast<const GPUDeviceContext*>(ctx)->stream();
  if (gs == nullptr) {
    return errors::Internal("No gpu stream is available.");
  }
  *stream = gs;
  if (dst != nullptr) {
    if (src.dtype() != dst->dtype()) {
      return errors::Internal("Can't copy a tensor of ",
                              DataTypeString(src.dtype()), " into a tensor of ",
                              DataTypeString(dst->dtype()));
    }
    if (src.TotalBytes() != dst->TotalBytes()) {
      return errors::Internal("Can't copy ", src.TotalBytes(),
                              " bytes of a tensor into another with ",
                              dst->TotalBytes(), " bytes buffer.");
    }
    if ((src.TotalBytes() > 0) && !src.IsInitialized()) {
      return errors::Internal("Src tensor is not initialized.");
    }
    if ((dst->TotalBytes() > 0) && !dst->IsInitialized()) {
      return errors::Internal("Dst tensor is not initialized.");
    }
  }
  if (!DMAHelper::CanUseDMA(&src)) {
    return errors::Internal("GPU copy from non-DMA ",
                            DataTypeString(src.dtype()), "tensor");
  }
  return Status::OK();
}

void* GetBase(const Tensor* src) {
  return const_cast<void*>(DMAHelper::base(src));
}

void* GetBase(Tensor* dst) { return DMAHelper::base(dst); }

/*static*/
void GPUUtil::SetProtoFromGPU(const Tensor& tensor, Device* dev,
                              const DeviceContext* device_context,
                              TensorProto* proto, bool is_dead,
                              StatusCallback done) {
  VLOG(1) << "SetProtoFromGPU device_context " << device_context;
  const DeviceBase::GpuDeviceInfo* dev_info = nullptr;
  gpu::Stream* stream = nullptr;
  Status s =
      PrepareCopy(dev, device_context, tensor, nullptr, &dev_info, &stream);
  if (!s.ok()) {
    done(s);
    return;
  }

  // Tensor values need to be copied from GPU to CPU ram so that
  // we can build the protobuf response for a RecvTensor RPC.
  // "device context" identifies the stream where the _Send op executed.
  proto->set_dtype(tensor.dtype());
  tensor.shape().AsProto(proto->mutable_tensor_shape());

  // Prepare a proto with the right data buf size, and DMA the data
  // over from the GPU buffer.  Note that 0-size tensors do not have a
  // backing buffer.
  Allocator* alloc = nullptr;
  char* buf = nullptr;
  const int64 total_bytes = is_dead ? 0 : tensor.TotalBytes();
  if (total_bytes > 0) {
    port::Tracing::ScopedAnnotation annotation("SetProtoFromGPU");
    alloc = ProcessState::singleton()->GetCUDAHostAllocator(0);
    buf = alloc->Allocate<char>(total_bytes);
    void* src_ptr = GetBase(&tensor);
    DeviceMemoryBase gpu_src_ptr(src_ptr, total_bytes);
    stream->ThenMemcpy(buf, gpu_src_ptr, total_bytes);
  }
  // Use of tensor may outlive stack scope, so keep a ref.
  TensorReference tensor_ref(tensor);
  dev_info->event_mgr->ThenExecute(stream, [stream, done, proto, buf,
                                            total_bytes, alloc, tensor_ref]() {
    if (!stream->ok()) {
      LOG(FATAL) << "SetProtoFromGPU: GPU Memcpy failed";
    }
    tensor_ref.Unref();
    if (total_bytes > 0) {
      port::CopyFromArray(proto->mutable_tensor_content(), buf, total_bytes);
      alloc->Deallocate<char>(buf, total_bytes);
    }
    done(Status::OK());
  });
}

// static
void GPUUtil::DeviceToDeviceCopy(DeviceContext* send_dev_context,
                                 DeviceContext* recv_dev_context, Device* src,
                                 Device* dst,
                                 AllocatorAttributes src_alloc_attr,
                                 AllocatorAttributes dst_alloc_attr,
                                 const Tensor* input, Tensor* output,
                                 StatusCallback done) {
  const DeviceBase::GpuDeviceInfo* dev_info = nullptr;
  gpu::Stream* stream = nullptr;
  Status s =
      PrepareCopy(src, send_dev_context, *input, output, &dev_info, &stream);
  if (!s.ok()) {
    done(s);
    return;
  }

  const int64 total_bytes = input->TotalBytes();
  if (total_bytes > 0) {
    void* src_ptr = GetBase(input);
    DeviceMemoryBase gpu_src_ptr(src_ptr, total_bytes);
    void* dst_ptr = GetBase(output);
    DeviceMemoryBase gpu_dst_ptr(dst_ptr, total_bytes);
    VLOG(2) << "src_ptr " << src_ptr << " dst_ptr " << dst_ptr;
    stream->ThenMemcpy(&gpu_dst_ptr, gpu_src_ptr, total_bytes);
  }

  // Use of input may outlive stack scope, so keep a ref.
  TensorReference input_ref(*input);
  dev_info->event_mgr->ThenExecute(stream, [done, stream, input_ref]() {
    input_ref.Unref();
    if (!stream->ok()) {
      LOG(FATAL) << "GPU->GPU Memcpy failed";
    }
    done(Status::OK());
  });
  send_dev_context->MaintainLifetimeOnStream(input, stream);
}

static CopyTensor::Registration register_gpu_gpu_copy(
    DEVICE_GPU, DEVICE_GPU, GPUUtil::DeviceToDeviceCopy);

// static
void GPUUtil::CopyGPUTensorToCPU(Device* gpu_device,
                                 const DeviceContext* device_context,
                                 const Tensor* gpu_tensor, Tensor* cpu_tensor,
                                 StatusCallback done) {
  VLOG(1) << "CopyGPUTensorToCPU";
  const DeviceBase::GpuDeviceInfo* dev_info = nullptr;
  gpu::Stream* stream = nullptr;
  Status s = PrepareCopy(gpu_device, device_context, *gpu_tensor, cpu_tensor,
                         &dev_info, &stream);
  if (!s.ok()) {
    done(s);
    return;
  }
  const int64 total_bytes = gpu_tensor->TotalBytes();
  if (total_bytes > 0) {
    void* src_ptr = GetBase(gpu_tensor);
    DeviceMemoryBase gpu_src_ptr(src_ptr, total_bytes);
    void* dst_ptr = GetBase(cpu_tensor);
    stream->ThenMemcpy(dst_ptr, gpu_src_ptr, total_bytes);
  }
  dev_info->event_mgr->ThenExecute(stream, [stream, done]() {
    if (!stream->ok()) {
      LOG(FATAL) << "GPU->CPU Memcpy failed";
    }
    done(Status::OK());
  });
}

/*  static */
void GPUUtil::CopyCPUTensorToGPU(const Tensor* cpu_tensor,
                                 const DeviceContext* device_context,
                                 Device* gpu_device, Tensor* gpu_tensor,
                                 StatusCallback done) {
  VLOG(1) << "CopyCPUTensorToGPU";
  const DeviceBase::GpuDeviceInfo* dev_info = nullptr;
  gpu::Stream* stream = nullptr;
  Status s = PrepareCopy(gpu_device, device_context, *cpu_tensor, gpu_tensor,
                         &dev_info, &stream);
  if (!s.ok()) {
    done(s);
    return;
  }
  const int64 total_bytes = cpu_tensor->TotalBytes();
  // Note that 0-size tensors have no backing buffer.
  if (total_bytes > 0) {
    void* src_ptr = GetBase(cpu_tensor);
    void* dst_ptr = GetBase(gpu_tensor);
    DeviceMemoryBase gpu_dst_ptr(dst_ptr, total_bytes);
    stream->ThenMemcpy(&gpu_dst_ptr, src_ptr, total_bytes);
  }
  // Use of cpu_tensor may outlive stack scope, so keep a ref.
  TensorReference input_ref(*cpu_tensor);
  dev_info->event_mgr->ThenExecute(stream, [stream, done, input_ref]() {
    input_ref.Unref();
    if (!stream->ok()) {
      LOG(FATAL) << "CPU->GPU Memcpy failed";
    }
    done(Status::OK());
  });
}

Status GPUUtil::Sync(Device* gpu_device) {
  VLOG(1) << "GPUUtil::Sync";
  auto* dev_info = gpu_device->tensorflow_gpu_device_info();
  if (!dev_info) {
    return errors::Internal("Failed to find dest device GPUDeviceInfo");
  }
  dev_info->stream->BlockHostUntilDone();
  if (!dev_info->stream->ok()) {
    LOG(FATAL) << "GPU sync failed";
  }
  return Status::OK();
}

Status GPUUtil::SyncAll(Device* gpu_device) {
  VLOG(1) << "GPUUtil::SyncAll";
  auto* dev_info = gpu_device->tensorflow_gpu_device_info();
  if (!dev_info) {
    return errors::Internal("Failed to find dest device GPUDeviceInfo");
  }
  if (!dev_info->stream->parent()->SynchronizeAllActivity() ||
      !dev_info->stream->ok()) {
    LOG(FATAL) << "GPU sync failed";
  }
  return Status::OK();
}

string GPUUtil::MemoryDebugString(const Device* device, Tensor* tensor) {
  string ret;
  CHECK(tensor);
  const int64 num_bytes = std::min<int64>(
      FLAGS_brain_gpu_util_debug_string_maxlen, tensor->TotalBytes());
  void* ptr = (num_bytes > 0) ? GetBase(tensor) : nullptr;
  strings::Appendf(&ret, "%p:", ptr);
  if (num_bytes > 0) {
    auto* dev_info = device->tensorflow_gpu_device_info();
    if (!dev_info) {
      strings::StrAppend(
          &ret, PrintMemory(reinterpret_cast<const char*>(ptr), num_bytes));
    } else {
      string buf;
      buf.resize(num_bytes);
      DeviceMemoryBase gpu_ptr(ptr, num_bytes);
      Status s = dev_info->stream->parent()->SynchronousMemcpyD2H(
          gpu_ptr, num_bytes, gtl::string_as_array(&buf));
      strings::StrAppend(&ret,
                         PrintMemory(gtl::string_as_array(&buf), num_bytes));
    }
  }
  return ret;
}

// TODO(pbar) Checksum is called from places without a valid device context.
uint64 GPUUtil::Checksum(Device* gpu_device,
                         const DeviceContext* device_context,
                         const Tensor& tensor) {
  Tensor copy(tensor.dtype(), tensor.shape());
  Status s;
  Notification n;
  CopyGPUTensorToCPU(gpu_device, device_context, &tensor, &copy,
                     [&s, &n](Status status) {
                       s.Update(status);
                       n.Notify();
                     });
  n.WaitForNotification();
  CHECK(s.ok()) << s;
  return Checksum(copy);
}

uint64 GPUUtil::Checksum(const Tensor& tensor) {
  const float* fptr = reinterpret_cast<const float*>(GetBase(&tensor));
  size_t num_bytes = tensor.TotalBytes();
  size_t num_floats = num_bytes / sizeof(float);
  for (size_t i = 0; i < num_floats; ++i) {
    CHECK(!std::isnan(fptr[i])) << " i " << i;
  }
  // TODO(tucker): consider using crc32c instead.
  return Hash64(reinterpret_cast<const char*>(GetBase(&tensor)),
                tensor.TotalBytes(), 0);
}

}  // namespace tensorflow
