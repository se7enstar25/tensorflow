/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_CORE_COMMON_RUNTIME_DEVICE_DEVICE_ID_H_
#define TENSORFLOW_CORE_COMMON_RUNTIME_DEVICE_DEVICE_ID_H_

#include "tensorflow/core/lib/gtl/int_type.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {

// There are three types of device ids:
// - *physical* device id: this is the integer index of a device in the
//   physical machine, it can be filtered (for e.g. using environment variable
//   CUDA_VISIBLE_DEVICES when using CUDA). Note that this id is not visible to
//   Tensorflow, but result after filtering is visible to TF and is called
//   platform device id as below.
//   For CUDA, see
//   http://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#env-vars
//   for more details.
// - *platform* device id (also called *visible* device id in
//   third_party/tensorflow/core/protobuf/config.proto): this is the id that is
//   visible to Tensorflow after filtering (for e.g. by CUDA_VISIBLE_DEVICES).
//   For CUDA, this id is generated by the CUDA GPU driver. It starts from 0
//   and is used for CUDA API calls like cuDeviceGet().
// - TF device id (also called *virtual* device id in
//   third_party/tensorflow/core/protobuf/config.proto): this is the id that
//   Tensorflow generates and exposes to its users. It is the id in the <id>
//   field of the device name "/device:GPU:<id>", and is also the identifier of
//   a BaseGPUDevice. Note that the configuration allows us to create multiple
//   BaseGPUDevice per GPU hardware in order to use multi CUDA streams on the
//   hardware, so the mapping between TF GPU id and platform GPU id is not a 1:1
//   mapping, see the example below.
//
// For example, assuming that in the machine we have GPU device with index 0, 1,
// 2 and 3 (physical GPU id). Setting "CUDA_VISIBLE_DEVICES=1,2,3" will create
// the following mapping between platform GPU id and physical GPU id:
//
//        platform GPU id ->  physical GPU id
//                 0  ->  1
//                 1  ->  2
//                 2  ->  3
//
// Note that physical GPU id 0 is invisible to TF so there is no mapping entry
// for it.
//
// Assuming we configure the Session to create one BaseGPUDevice per GPU
// hardware, then setting GPUOptions::visible_device_list to "2,0" will create
// the following mapping between TF device id and platform device id:
//
//                  TF GPU id  ->  platform GPU ID
//      0 (i.e. /device:GPU:0) ->  2
//      1 (i.e. /device:GPU:1) ->  0
//
// Note that platform device id 1 is filtered out by
// GPUOptions::visible_device_list, so it won't be used by the TF process.
//
// On the other hand, if we configure it to create 2 BaseGPUDevice per GPU
// hardware, then setting GPUOptions::visible_device_list to "2,0" will create
// the following mapping between TF device id and platform device id:
//
//                  TF GPU id  ->  platform GPU ID
//      0 (i.e. /device:GPU:0) ->  2
//      1 (i.e. /device:GPU:1) ->  2
//      2 (i.e. /device:GPU:2) ->  0
//      3 (i.e. /device:GPU:3) ->  0
//
// We create strong-typed integer classes for both TF device id and platform
// device id to minimize programming errors and improve code readability. Except
// for the StreamExecutor interface (as we don't change its API), whenever we
// need a TF device id (or platform device id) we should use TfDeviceId (or
// PlatformDeviceId) instead of a raw integer.
TF_LIB_GTL_DEFINE_INT_TYPE(TfDeviceId, int32);
TF_LIB_GTL_DEFINE_INT_TYPE(PlatformDeviceId, int32);

}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_COMMON_RUNTIME_DEVICE_DEVICE_ID_H_
