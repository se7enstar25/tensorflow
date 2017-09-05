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

#ifndef THIRD_PARTY_TENSORFLOW_CORE_GRAPPLER_UTILS_FRAME_H_
#define THIRD_PARTY_TENSORFLOW_CORE_GRAPPLER_UTILS_FRAME_H_

#include <unordered_map>
#include "tensorflow/core/framework/graph.pb.h"

namespace tensorflow {
namespace grappler {

// Returns the number of frames present in the graph, and populates
// the 'frames' argument with the collection of frames (denoted by their
// frame ids) in the outermost-to-innermost order. Frame ids are arbitrary.
int IdentifyFrames(
    const GraphDef& graph,
    std::unordered_map<const NodeDef*, std::vector<int>>* frames);

}  // namespace grappler
}  // namespace tensorflow

#endif  // THIRD_PARTY_TENSORFLOW_CORE_GRAPPLER_UTILS_FRAME_H_
