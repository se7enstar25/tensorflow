/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/lite/java/src/main/native/op_resolver.h"
#include "tensorflow/lite/mutable_op_resolver.h"

// This method is generated by `gen_selected_ops`.
// TODO(b/153652701): Instead of relying on a global method, make
// `gen_selected_ops` generating a header file with custom namespace.
void RegisterSelectedOps(::tflite::MutableOpResolver* resolver);

namespace tflite {
// This interface is the unified entry point for creating op resolver
// regardless if selective registration is being used. C++ client will call
// this method directly and Java client will call this method indirectly via
// JNI code in interpreter_jni.cc.
std::unique_ptr<OpResolver> CreateOpResolver() {
  std::unique_ptr<MutableOpResolver> resolver =
      std::unique_ptr<MutableOpResolver>(new MutableOpResolver());
  RegisterSelectedOps(resolver.get());
  return std::move(resolver);
}

}  // namespace tflite
