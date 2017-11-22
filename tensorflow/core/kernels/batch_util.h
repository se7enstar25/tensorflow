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
#ifndef THIRD_PARTY_TENSORFLOW_CORE_KERNELS_BATCH_UTIL_H_
#define THIRD_PARTY_TENSORFLOW_CORE_KERNELS_BATCH_UTIL_H_

#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/lib/core/status.h"

namespace tensorflow {
namespace batch_util {

// Copies element into the index^th slice of parent (in the 0th dimension).
//
// NOTE(mrry): The `element` argument is taken by value. Use `std::move()`
// to move the `element` argument into this function, and the implementation
// may be able to optimize the copy to a move. This is particularly important
// for DT_STRING tensors.
Status CopyElementToSlice(Tensor element, Tensor* parent, int64 index);

}  // namespace batch_util
}  // namespace tensorflow

#endif  // THIRD_PARTY_TENSORFLOW_CORE_KERNELS_BATCH_UTIL_H_
