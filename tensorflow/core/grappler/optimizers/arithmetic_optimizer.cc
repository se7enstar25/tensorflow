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

#include "tensorflow/core/grappler/optimizers/arithmetic_optimizer.h"
#include <unordered_map>
#include <unordered_set>
#include "tensorflow/core/framework/attr_value.pb.h"
#include "tensorflow/core/framework/node_def.pb.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/tensor_shape.pb.h"
#include "tensorflow/core/grappler/grappler_item.h"
#include "tensorflow/core/grappler/op_types.h"
#include "tensorflow/core/lib/strings/strcat.h"
#include "tensorflow/core/platform/tensor_coding.h"

namespace tensorflow {
namespace grappler {

class UniqueNodes {
 public:
  NodeDef* FindOrAddRepresentative(NodeDef* node) {
    std::size_t sig = ComputeSignature(*node);
    std::vector<NodeDef*>& candidates = rep_[sig];
    for (auto& candidate : candidates) {
      if (SameNode(*candidate, *node)) {
        return candidate;
      }
    }
    candidates.push_back(node);
    return node;
  }

 private:
  std::size_t ComputeSignature(const NodeDef& node) const;
  bool SameNode(const NodeDef& node1, const NodeDef& node2) const;

  std::unordered_map<std::size_t, std::vector<NodeDef*>> rep_;
};

std::size_t UniqueNodes::ComputeSignature(const NodeDef& node) const {
  std::size_t h = std::hash<string>{}(node.op());
  h ^= std::hash<string>{}(node.device());
  for (const auto& input : node.input()) {
    int pos;
    string node_name = ParseNodeName(input, &pos);
    h ^= std::hash<string>{}(node_name);
    h ^= static_cast<std::size_t>(pos);
  }
  for (const auto& attr : node.attr()) {
    h ^= std::hash<string>{}(attr.first);
    string tmp;
    attr.second.AppendToString(&tmp);
    h ^= std::hash<string>{}(tmp);
  }
  return h;
}

bool UniqueNodes::SameNode(const NodeDef& node1, const NodeDef& node2) const {
  if (node1.op() != node2.op()) {
    return false;
  }
  if (node1.device() != node2.device()) {
    return false;
  }
  if (node1.input_size() != node2.input_size()) {
    return false;
  }
  if (node1.attr_size() != node2.attr_size()) {
    return false;
  }

  // Compare inputs.
  const OpDef* op_def = nullptr;
  Status status = OpRegistry::Global()->LookUpOpDef(node1.op(), &op_def);
  const bool is_commutative = status.ok() && op_def->is_commutative();
  if (is_commutative) {
    std::vector<string> inputs1(node1.input().begin(), node1.input().end());
    std::vector<string> inputs2(node2.input().begin(), node2.input().end());
    std::sort(inputs1.begin(), inputs1.end());
    std::sort(inputs2.begin(), inputs2.end());
    return inputs1 == inputs2;
  } else {
    std::vector<string> regular_inputs1;
    std::vector<string> regular_inputs2;
    std::vector<string> ctrl_inputs1;
    std::vector<string> ctrl_inputs2;
    for (int index = 0; index < node1.input_size(); ++index) {
      if (IsControlInput(node1.input(index))) {
        ctrl_inputs1.push_back(node1.input(index));
        ctrl_inputs2.push_back(node2.input(index));

      } else {
        regular_inputs1.push_back(node1.input(index));
        regular_inputs2.push_back(node2.input(index));
      }
    }
    if (regular_inputs1 != regular_inputs2) {
      return false;
    }
    std::sort(ctrl_inputs1.begin(), ctrl_inputs1.end());
    std::sort(ctrl_inputs2.begin(), ctrl_inputs2.end());
    if (ctrl_inputs1 != ctrl_inputs2) {
      return false;
    }
  }

  // Compare attributes.
  for (const auto& attr1 : node1.attr()) {
    auto it = node2.attr().find(attr1.first);
    if (it == node2.attr().end()) {
      return false;
    }
    const auto& attr2 = *it;
    string val1;
    attr1.second.AppendToString(&val1);
    string val2;
    attr2.second.AppendToString(&val2);
    if (val1 != val2) {
      return false;
    }
  }

  return true;
}

bool ArithmeticOptimizer::CanDedup(const NodeDef& node) const {
  if (nodes_to_preserve_.find(node.name()) != nodes_to_preserve_.end()) {
    return false;
  }
  if (IsEnter(node) || IsExit(node) || IsPlaceholder(node)) {
    return false;
  }
  if (node.device().find("SPU") != string::npos) {
    return false;
  }
  const OpDef* op_def = nullptr;
  Status status = OpRegistry::Global()->LookUpOpDef(node.op(), &op_def);
  if (!status.ok()) {
    return false;
  }
  if (op_def->is_stateful()) {
    return false;
  }
  // Don't consolidate ops such as AssignAdd
  for (const auto& input : op_def->input_arg()) {
    if (input.is_ref()) {
      return false;
    }
  }
  return true;
}

void ArithmeticOptimizer::DedupComputations(GraphDef* optimized_graph) const {
  NodeMap map(optimized_graph);
  bool stop = true;
  std::set<int> duplicates;
  do {
    stop = true;
    UniqueNodes nodes;
    for (int i = 0; i < optimized_graph->node_size(); ++i) {
      if (duplicates.find(i) != duplicates.end()) {
        continue;
      }
      NodeDef* node = optimized_graph->mutable_node(i);
      if (!CanDedup(*node)) {
        continue;
      }
      NodeDef* rep = nodes.FindOrAddRepresentative(node);
      if (rep == node) {
        continue;
      }
      const std::set<NodeDef*>& fanouts = map.GetOutputs(node->name());
      for (NodeDef* fanout : fanouts) {
        for (string& name : *fanout->mutable_input()) {
          int position;
          string nodename = ParseNodeName(name, &position);
          if (nodename == node->name()) {
            if (position > 0) {
              name = strings::StrCat(rep->name(), ":", position);
            } else if (position == 0) {
              name = rep->name();
            } else {
              name = strings::StrCat("^", rep->name());
            }
            map.AddOutput(rep->name(), fanout->name());
          }
        }
      }
      duplicates.insert(i);
      stop = false;
    }
  } while (!stop);

  // Delete duplicates
  if (!duplicates.empty()) {
    int last = optimized_graph->node_size() - 1;
    for (auto it = duplicates.rbegin(); it != duplicates.rend(); ++it) {
      int index = *it;
      optimized_graph->mutable_node()->SwapElements(index, last);
      last--;
    }
    optimized_graph->mutable_node()->DeleteSubrange(last + 1,
                                                    duplicates.size());
  }
}

static bool AreInversePermutations(gtl::ArraySlice<int32> a,
                                   gtl::ArraySlice<int32> b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (int i = 0; i < a.size(); ++i) {
    if (a[b[i]] != i) {
      return false;
    }
  }
  return true;
}

// Extract int32 values from a Const op to `int32_values`. Returns true if
// succeeds.
static bool Int32ValuesFromNode(const NodeDef& node,
                                std::vector<int>* int32_values) {
  if (node.op() != "Const") {
    return false;
  }

  if (node.attr().at("dtype").type() != DT_INT32) {
    return false;
  }

  // TensorProto represents the content of the tensor in either <type>_val or
  // tensor_content.
  const TensorProto& tensor = node.attr().at("value").tensor();
  if (tensor.int_val_size() > 0 && tensor.has_tensor_shape()) {
    // When tensor_shape is set, theoretically the representation of the data
    // could be compressed. So, before copying int_val to the returned vector,
    // make sure no compression happens.
    const TensorShapeProto& shape = tensor.tensor_shape();
    if (shape.dim_size() == 1 && shape.dim(0).size() == tensor.int_val_size()) {
      int32_values->insert(int32_values->end(), tensor.int_val().begin(),
                           tensor.int_val().end());
    }
    return true;
  }

  const auto tensor_content_size = tensor.tensor_content().size();
  if (tensor_content_size > 0) {
    CHECK_EQ(0, tensor_content_size % sizeof(int32))
        << "tensor_content_size (" << tensor_content_size
        << ") is not a multiple of " << sizeof(int32);
    int32_values->resize(tensor_content_size / sizeof(int32));
    port::CopyToArray(tensor.tensor_content(),
                      reinterpret_cast<char*>(int32_values->data()));
    return true;
  }

  return false;
}

bool ArithmeticOptimizer::TrySimplifyAndReplaceUses(const NodeDef* node,
                                                    NodeMap* node_map) const {
  bool changed = false;
  if (node->op() == "Transpose") {
    const NodeDef* input = node_map->GetNode(node->input()[0]);
    if (input->op() == "Transpose") {
      const NodeDef* node_perm = node_map->GetNode(node->input()[1]);
      const NodeDef* input_perm = node_map->GetNode(input->input()[1]);
      std::vector<int> node_perm_values;
      std::vector<int> input_perm_values;
      if (Int32ValuesFromNode(*node_perm, &node_perm_values) &&
          Int32ValuesFromNode(*input_perm, &input_perm_values) &&
          AreInversePermutations(node_perm_values, input_perm_values)) {
        // Copy the result of GetOutputs to consumers so avoid modifying NodeMap
        // while iterating it.
        std::set<NodeDef*> consumers = node_map->GetOutputs(node->name());
        for (NodeDef* consumer : consumers) {
          // Update `consumer`'s use of `node` to `input`'s operand.
          protobuf::RepeatedPtrField<string>* inputs_of_consumer =
              consumer->mutable_input();
          for (int i = 0; i < consumer->input_size(); ++i) {
            if (NodeName(inputs_of_consumer->Get(i)) == node->name()) {
              *inputs_of_consumer->Mutable(i) = input->input()[0];
            }
          }
          node_map->UpdateInput(consumer->name(), node->name(),
                                input->input()[0]);
          VLOG(2) << "Update input " << node->name() << " of "
                  << consumer->name() << " to " << input->input()[0];
          changed = true;
        }
      }
    }
  }
  return changed;
}

namespace {
// A vector with a set. The set stores the same elements as the vector, and
// quickly answers whether a value is in the vector. Duplicated elements are not
// allowed for now.
template <class T>
class SetVector {
 public:
  void PushBack(const T& value) {
    CHECK(!Exists(value)) << "Value " << value << " is already in the set.";
    set_.insert(value);
    vector_.push_back(value);
  }

  T PopBack() {
    T back = vector_.back();
    set_.erase(back);
    vector_.pop_back();
    return back;
  }

  bool Exists(const T& value) const { return set_.count(value); }

  bool Empty() const { return vector_.empty(); }

 private:
  std::unordered_set<T> set_;
  std::vector<T> vector_;
};
}  // namespace

void ArithmeticOptimizer::RemoveRedundantTransposes(
    GraphDef* optimized_graph) const {
  NodeMap node_map(optimized_graph);
  SetVector<const NodeDef*> nodes_to_simplify;
  for (int i = 0; i < optimized_graph->node_size(); ++i) {
    nodes_to_simplify.PushBack(optimized_graph->mutable_node()->Mutable(i));
  }
  while (!nodes_to_simplify.Empty()) {
    const NodeDef* node = nodes_to_simplify.PopBack();
    if (TrySimplifyAndReplaceUses(node, &node_map)) {
      // The consumers of `node` are modified when TrySimplifyAndReplaceUses
      // returns true. Re-push them into `nodes_to_simplify` for further
      // optimizations.
      for (NodeDef* consumer : node_map.GetOutputs(node->name())) {
        if (!nodes_to_simplify.Exists(consumer)) {
          nodes_to_simplify.PushBack(consumer);
        }
      }
    }
  }
}

Status ArithmeticOptimizer::Optimize(Cluster* /*cluster*/,
                                     const GrapplerItem& item,
                                     GraphDef* optimized_graph) {
  *optimized_graph = item.graph;
  nodes_to_preserve_ = item.NodesToPreserve();

  DedupComputations(optimized_graph);
  RemoveRedundantTransposes(optimized_graph);

  return Status::OK();
}

void ArithmeticOptimizer::Feedback(Cluster* /*cluster*/,
                                   const GrapplerItem& /*item*/,
                                   const GraphDef& /*optimized_graph*/,
                                   double /*result*/) {
  // Nothing to do for ArithmeticOptimizer.
}

}  // end namespace grappler
}  // end namespace tensorflow
