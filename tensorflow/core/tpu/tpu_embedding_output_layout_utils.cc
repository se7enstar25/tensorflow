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

#include "tensorflow/core/tpu/tpu_embedding_output_layout_utils.h"

#include <vector>

namespace tensorflow {
namespace tpu {

Status ComputeOutputTensorShapes(
    const tensorflow::tpu::TPUEmbeddingConfiguration& config,
    std::vector<TensorShapeProto>* shapes) {
  if (config.feature_descriptor_size() > 0) {
    for (const TPUEmbeddingConfiguration::FeatureDescriptor& feature :
         config.feature_descriptor()) {
      TensorShapeProto shape;
      for (int32 input_shape : feature.input_shape()) {
        auto* dim = shape.add_dim();
        dim->set_size(input_shape);
      }
      shape.add_dim()->set_size(
          config.table_descriptor(feature.table_id()).dimension());
      shape.mutable_dim(0)->set_size(shape.dim(0).size());
      shapes->push_back(shape);
    }
  } else {
    const int batch_size = config.batch_size_per_tensor_core();
    for (const TPUEmbeddingConfiguration::TableDescriptor& table :
         config.table_descriptor()) {
      TensorShapeProto shape;
      auto* dim0 = shape.add_dim();
      dim0->set_size(table.num_features() * batch_size);
      auto* dim1 = shape.add_dim();
      dim1->set_size(table.dimension());
      shapes->push_back(shape);
    }
  }
  return OkStatus();
}

}  // namespace tpu
}  // namespace tensorflow
