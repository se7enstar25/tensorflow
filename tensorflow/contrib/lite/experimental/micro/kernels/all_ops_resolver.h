/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.
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
#ifndef TENSORFLOW_CONTRIB_LITE_EXPERIMENTAL_MICRO_KERNELS_ALL_OPS_RESOLVER_H_
#define TENSORFLOW_CONTRIB_LITE_EXPERIMENTAL_MICRO_KERNELS_ALL_OPS_RESOLVER_H_

#include "tensorflow/contrib/lite/experimental/micro/compatibility.h"
#include "tensorflow/contrib/lite/experimental/micro/micro_mutable_op_resolver.h"

namespace tflite {
namespace ops {
namespace micro {

class AllOpsResolver : public MicroMutableOpResolver {
 public:
  AllOpsResolver();

 private:
  TF_LITE_REMOVE_VIRTUAL_DELETE
};

}  // namespace micro
}  // namespace ops
}  // namespace tflite

#endif  // TENSORFLOW_CONTRIB_LITE_EXPERIMENTAL_MICRO_KERNELS_ALL_OPS_RESOLVER_H_
