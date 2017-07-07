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

#include "tensorflow/core/grappler/optimizers/graph_rewriter.h"
#include <unordered_map>
#include <unordered_set>
#include "tensorflow/core/framework/node_def.pb.h"
#include "tensorflow/core/grappler/grappler_item.h"
#include "tensorflow/core/grappler/utils.h"

namespace tensorflow {
namespace grappler {

GraphRewriter::GraphRewriter(const GrapplerItem& item) {
  for (auto& node : item.graph.node()) {
    nodes_[node.name()] = &node;
  }

  for (auto& node : item.graph.node()) {
    RecordControlDependencyDrivers(node);
  }
}

void GraphRewriter::ForwardInputs(
    const NodeDef& original_node,
    const std::unordered_set<const NodeDef*>& nodes_to_delete,
    NodeDef* new_node) {
  ForwardInputsInternal(original_node, nodes_to_delete, new_node);
  if (!new_node->name().empty()) {
    optimized_nodes_[new_node->name()] = new_node;
  }
}

bool GraphRewriter::DrivesControlDependency(const NodeDef& node) const {
  return control_dependency_drivers_.find(&node) !=
         control_dependency_drivers_.end();
}

bool GraphRewriter::IsDrivenByControlDependency(const NodeDef& node) const {
  for (const auto& input : node.input()) {
    CHECK(!input.empty());
    if (input[0] == '^') {
      return true;
    }
  }
  return false;
}

void GraphRewriter::RecordControlDependencyDrivers(const NodeDef& node) {
  for (const auto& input : node.input()) {
    int position = 0;
    string input_node_name = ParseNodeName(input, &position);
    if (position < 0) {
      // This is a control edge
      auto itr = nodes_.find(input_node_name);
      CHECK(itr != nodes_.end());
      control_dependency_drivers_.insert(itr->second);
    }
  }
}

void GraphRewriter::ForwardInputsInternal(
    const NodeDef& node,
    const std::unordered_set<const NodeDef*>& nodes_to_delete,
    NodeDef* new_node) {
  // To speed things up, use the optimized version of the node if
  // available.
  auto itr = optimized_nodes_.find(node.name());
  if (itr != optimized_nodes_.end()) {
    for (const string& input : itr->second->input()) {
      *new_node->add_input() = input;
    }
    return;
  }
  for (const auto& input : node.input()) {
    string input_node_name = NodeName(input);
    auto itr = nodes_.find(input_node_name);
    if (itr == nodes_.end()) {
      // Invalid input, preserve it as is.
      *new_node->add_input() = input;
      continue;
    }
    const NodeDef* input_node = itr->second;
    if ((input_node->device().empty() || node.device().empty() ||
         input_node->device() == node.device()) &&
        nodes_to_delete.find(input_node) != nodes_to_delete.end()) {
      ForwardInputsInternal(*input_node, nodes_to_delete, new_node);
    } else {
      *new_node->add_input() = input;
    }
  }
}

}  // end namespace grappler
}  // end namespace tensorflow
