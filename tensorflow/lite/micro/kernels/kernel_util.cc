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

#include "tensorflow/lite/micro/kernels/kernel_util.h"

namespace tflite {
namespace micro {

// Returns the TfLiteEvalTensor struct for a given input index in a node.
const TfLiteEvalTensor* GetEvalInput(const TfLiteContext* context,
                                     const TfLiteNode* node, int index) {
  TFLITE_DCHECK(context != nullptr);
  TFLITE_DCHECK(node != nullptr);
  return context->GetEvalTensor(context, node->inputs->data[index]);
}

// Returns the TfLiteEvalTensor struct for a given output index in a node.
TfLiteEvalTensor* GetEvalOutput(const TfLiteContext* context,
                                const TfLiteNode* node, int index) {
  TFLITE_DCHECK(context != nullptr);
  TFLITE_DCHECK(node != nullptr);
  return context->GetEvalTensor(context, node->outputs->data[index]);
}

const RuntimeShape GetTensorShape(const TfLiteEvalTensor* tensor) {
  TFLITE_DCHECK(tensor != nullptr);
  TfLiteIntArray* dims = tensor->dims;
  const int dims_size = dims->size;
  const int32_t* dims_data = reinterpret_cast<const int32_t*>(dims->data);
  return RuntimeShape(dims_size, dims_data);
}

}  // namespace micro
}  // namespace tflite
