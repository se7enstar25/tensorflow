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

#include "tensorflow/contrib/lite/allocation.h"

#include <cassert>

namespace tflite {

MMAPAllocation::MMAPAllocation(const char* filename,
                               ErrorReporter* error_reporter)
    : Allocation(error_reporter), mmapped_buffer_(nullptr) {
  // The disabled variant should never be created.
  assert(false);
}

MMAPAllocation::~MMAPAllocation() {}

const void* MMAPAllocation::base() const { return nullptr; }

size_t MMAPAllocation::bytes() const { return 0; }

bool MMAPAllocation::valid() const { return false; }

bool MMAPAllocation::IsSupported() { return false; }

}  // namespace tflite
