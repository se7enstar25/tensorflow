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
#ifndef TENSORFLOW_LITE_EXPERIMENTAL_DELEGATES_COREML_BUILDERS_RESHAPE_OP_BUILDER_H_
#define TENSORFLOW_LITE_EXPERIMENTAL_DELEGATES_COREML_BUILDERS_RESHAPE_OP_BUILDER_H_

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/experimental/delegates/coreml/builders/op_builder.h"

namespace tflite {
namespace delegates {
namespace coreml {
// Builder for Reshape op in CoreML.
class ReshapeOpBuilder : public OpBuilder {
 public:
  explicit ReshapeOpBuilder(GraphBuilder* graph_builder)
      : OpBuilder(graph_builder) {}
  const char* DebugName() override;

  CoreML::Specification::NeuralNetworkLayer* Build() override;

  TfLiteStatus RegisterInputs(const TfLiteIntArray* inputs,
                              TfLiteContext* context) override;
  TfLiteStatus RegisterOutputs(const TfLiteIntArray* outputs,
                               TfLiteContext* context) override;

  // Sets output shape of the Core ML reshape layer, given output shape and
  // the input tensor's shape.
  void SetShapeFromTensor(const TfLiteTensor* output_shape,
                          const TfLiteIntArray* input_shape);
  void SetShapeFromIntArray(const TfLiteIntArray* output_shape,
                            const TfLiteIntArray* input_shape);

 private:
  std::vector<int> shape_;
  // When channel dimension is changed, reshape should be done with HWC layout,
  // thus transpose is required. (set with ReshapeLayerParams.mode)
  bool need_transpose_ = false;
};

}  // namespace coreml
}  // namespace delegates
}  // namespace tflite

#endif  // TENSORFLOW_LITE_EXPERIMENTAL_DELEGATES_COREML_BUILDERS_RESHAPE_OP_BUILDER_H_
