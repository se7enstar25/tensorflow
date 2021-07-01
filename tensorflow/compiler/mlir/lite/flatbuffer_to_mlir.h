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

#ifndef TENSORFLOW_COMPILER_MLIR_LITE_FLATBUFFER_TO_MLIR_H_
#define TENSORFLOW_COMPILER_MLIR_LITE_FLATBUFFER_TO_MLIR_H_

#include <string>

namespace mlir {
namespace TFL {

// Translates the given FlatBuffer filename or buffer into MLIR and returns
// translated MLIR as string.
std::string FlatBufferFileToMlir(const std::string& model_file_or_buffer,
                                 bool input_is_filepath);

}  // namespace TFL
}  // namespace mlir

#endif  // TENSORFLOW_COMPILER_MLIR_LITE_FLATBUFFER_TO_MLIR_H_
