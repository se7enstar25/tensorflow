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

#ifndef TENSORFLOW_GRAPPLER_OPTIMIZERS_CUSTOM_GRAPH_OPTIMIZER_H_
#define TENSORFLOW_GRAPPLER_OPTIMIZERS_CUSTOM_GRAPH_OPTIMIZER_H_

#include "tensorflow/core/grappler/optimizers/graph_optimizer.h"
#include "tensorflow/core/lib/core/status.h"

namespace tensorflow {
namespace grappler {

// A custom optimizer that can be registered.
class CustomGraphOptimizer : public GraphOptimizer {
 public:
  virtual ~CustomGraphOptimizer() {}
  virtual Status Init() = 0;
};

}  // end namespace grappler
}  // end namespace tensorflow

#endif  // TENSORFLOW_GRAPPLER_OPTIMIZERS_CUSTOM_GRAPH_OPTIMIZER_H_
