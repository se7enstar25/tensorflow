/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_CORE_GRAPPLER_OPTIMIZERS_DATA_AUTOTUNE_BUFFER_SIZES_H_
#define TENSORFLOW_CORE_GRAPPLER_OPTIMIZERS_DATA_AUTOTUNE_BUFFER_SIZES_H_

#include "tensorflow/core/grappler/optimizers/data/optimizer_base.h"

namespace tensorflow {
namespace grappler {

// This optimization does the following:
//
// 1. Adds `prefetch(AUTOTUNE)` after all asynchronous tf.data transformations
// (e.g. parallel map, parallel interleave, and map + batch) if they are not
// followed by a `prefetch` yet.
//
// 2. If there exists any `prefetch(buffer_size=N)` for `N>=0`,  it will replace
// the transformation with autotunable version of `prefetch` which uses N as
// the minimum size of the buffer.
//
// 3. Switches from using legacy autotuning for `prefetch` to using an algorithm
// based on the performance model.
class AutotuneBufferSizes : public TFDataOptimizerBase {
 public:
  AutotuneBufferSizes() = default;
  ~AutotuneBufferSizes() override = default;

  string name() const override { return "autotune_buffer_sizes"; };

  bool UsesFunctionLibrary() const override { return false; }

  Status Init(
      const tensorflow::RewriterConfig_CustomGraphOptimizer* config) override {
    return Status::OK();
  }

  Status OptimizeAndCollectStats(Cluster* cluster, const GrapplerItem& item,
                                 GraphDef* output,
                                 OptimizationStats* stats) override;

  void Feedback(Cluster* cluster, const GrapplerItem& item,
                const GraphDef& optimize_output, double result) override;
};

}  // namespace grappler
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_GRAPPLER_OPTIMIZERS_DATA_AUTOTUNE_BUFFER_SIZES_H_
