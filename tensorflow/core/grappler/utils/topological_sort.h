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

#ifndef TENSORFLOW_CORE_GRAPPLER_UTILS_TOPOLOGICAL_SORT_H_
#define TENSORFLOW_CORE_GRAPPLER_UTILS_TOPOLOGICAL_SORT_H_

#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/lib/core/status.h"

namespace tensorflow {
namespace grappler {

// Compute a topological ordering for the graph nodes.
Status ComputeTopologicalOrder(
    const GraphDef& graph, std::unordered_map<const NodeDef*, int>* topo_order,
    const std::vector<std::pair<const NodeDef*, const NodeDef*>>*
        extra_dependencies);

// Sort a graph in topological order.
Status TopologicalSort(GraphDef* graph);

}  // namespace grappler
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_GRAPPLER_UTILS_TOPOLOGICAL_SORT_H_
