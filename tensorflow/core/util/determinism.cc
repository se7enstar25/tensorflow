/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/util/determinism.h"

#include "tensorflow/core/util/env_var.h"

namespace tensorflow {

bool OpDeterminismRequired() {
  static bool op_determinism_required = [] {
    bool deterministic_ops = false;
    TF_CHECK_OK(tensorflow::ReadBoolFromEnvVar("TF_DETERMINISTIC_OPS",
                                               /*default_val=*/false,
                                               &deterministic_ops));
    return deterministic_ops;
  }();
  return op_determinism_required;
}

}  // namespace tensorflow
