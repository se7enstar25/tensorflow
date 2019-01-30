/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/grappler/mutable_graph_view.h"

#include <algorithm>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "tensorflow/core/framework/function.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/framework/node_def.pb.h"
#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/graph/tensor_id.h"
#include "tensorflow/core/grappler/op_types.h"
#include "tensorflow/core/grappler/utils.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/core/stringpiece.h"
#include "tensorflow/core/lib/gtl/map_util.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {
namespace grappler {

namespace {

bool IsTensorIdPortValid(const TensorId& tensor_id) {
  return tensor_id.index() >= Graph::kControlSlot;
}

bool IsTensorIdRegular(const TensorId& tensor_id) {
  return tensor_id.index() > Graph::kControlSlot;
}

bool IsTensorIdControlling(const TensorId& tensor_id) {
  return tensor_id.index() == Graph::kControlSlot;
}

bool IsOutputPortControlling(const MutableGraphView::OutputPort& port) {
  return port.port_id == Graph::kControlSlot;
}

// Determines if node is an Identity where it's first regular input is a Switch
// node.
bool IsIdentityConsumingSwitch(const MutableGraphView& graph,
                               const NodeDef& node) {
  if ((IsIdentity(node) || IsIdentityNSingleInput(node)) &&
      node.input_size() > 0) {
    TensorId tensor_id = ParseTensorName(node.input(0));
    if (IsTensorIdControlling(tensor_id)) {
      return false;
    }

    NodeDef* input_node = graph.GetNode(tensor_id.node());
    return IsSwitch(*input_node);
  }
  return false;
}

// Determines if node input can be deduped by regular inputs when used as a
// control dependency. Specifically, if a node is an Identity that leads to a
// Switch node, when used as a control dependency, that control dependency
// should not be deduped even though the same node is used as a regular input.
bool CanDedupControlWithRegularInput(const MutableGraphView& graph,
                                     const NodeDef& control_node) {
  return !IsIdentityConsumingSwitch(graph, control_node);
}

// Determines if node input can be deduped by regular inputs when used as a
// control dependency. Specifically, if a node is an Identity that leads to a
// Switch node, when used as a control dependency, that control dependency
// should not be deduped even though the same node is used as a regular input.
bool CanDedupControlWithRegularInput(const MutableGraphView& graph,
                                     absl::string_view control_node_name) {
  NodeDef* control_node = graph.GetNode(control_node_name);
  DCHECK(control_node != nullptr)
      << "Didn't find a node for control dependency: " << control_node_name;
  return CanDedupControlWithRegularInput(graph, *control_node);
}

Status MutationError(absl::string_view function_name, absl::string_view params,
                     absl::string_view msg) {
  return errors::InvalidArgument(absl::Substitute(
      "MutableGraphView::$0($1) error: $2.", function_name, params, msg));
}

using ErrorHandler = std::function<Status(absl::string_view)>;

ErrorHandler UpdateFanoutsError(absl::string_view from_node_name,
                                absl::string_view to_node_name) {
  return [from_node_name, to_node_name](absl::string_view msg) {
    string params = absl::Substitute("from_node_name='$0', to_node_name='$1'",
                                     from_node_name, to_node_name);
    return MutationError("UpdateFanouts", params, msg);
  };
}

Status CheckFaninIsRegular(const TensorId& fanin, ErrorHandler handler) {
  if (!IsTensorIdRegular(fanin)) {
    return handler(absl::Substitute("fanin '$0' must be a regular tensor id",
                                    fanin.ToString()));
  }
  return Status::OK();
}

Status CheckFaninIsValid(const TensorId& fanin, ErrorHandler handler) {
  if (!IsTensorIdPortValid(fanin)) {
    return handler(absl::Substitute("fanin '$0' must be a valid tensor id",
                                    fanin.ToString()));
  }
  return Status::OK();
}

Status CheckAddingFaninToSelf(absl::string_view node_name,
                              const TensorId& fanin, ErrorHandler handler) {
  if (node_name == fanin.node()) {
    return handler(
        absl::Substitute("can't add fanin '$0' to self", fanin.ToString()));
  }
  return Status::OK();
}

Status CheckRemovingFaninFromSelf(absl::string_view node_name,
                                  const TensorId& fanin, ErrorHandler handler) {
  if (node_name == fanin.node()) {
    return handler(absl::Substitute("can't remove fanin '$0' from self",
                                    fanin.ToString()));
  }
  return Status::OK();
}

string NodeMissingErrorMsg(absl::string_view node_name) {
  return absl::Substitute("node '$0' was not found", node_name);
}

Status CheckNodeExists(absl::string_view node_name, NodeDef* node,
                       ErrorHandler handler) {
  if (node == nullptr) {
    return handler(NodeMissingErrorMsg(node_name));
  }
  return Status::OK();
}

Status CheckPortRange(int port, int min, int max, ErrorHandler handler) {
  if (port < min || port > max) {
    if (max < min) {
      return handler("no available ports as node has no regular fanins");
    }
    return handler(
        absl::Substitute("port must be in range [$0, $1]", min, max));
  }
  return Status::OK();
}

}  // namespace

void MutableGraphView::AddAndDedupFanouts(NodeDef* node) {
  // TODO(lyandy): Checks for self loops, Switch control dependencies, fanins
  // exist, and all regular fanins come before controlling fanins.
  absl::flat_hash_set<absl::string_view> fanins;
  absl::flat_hash_set<absl::string_view> controlling_fanins;
  int max_input_port = -1;
  int pos = 0;
  const int last_idx = node->input_size() - 1;
  int last_pos = last_idx;
  while (pos <= last_pos) {
    TensorId tensor_id = ParseTensorName(node->input(pos));
    absl::string_view input_node_name = tensor_id.node();
    bool is_control_input = IsTensorIdControlling(tensor_id);
    bool can_dedup_control_with_regular_input =
        CanDedupControlWithRegularInput(*this, input_node_name);
    bool can_dedup_control =
        is_control_input && (can_dedup_control_with_regular_input ||
                             (!can_dedup_control_with_regular_input &&
                              controlling_fanins.contains(input_node_name)));
    if (!gtl::InsertIfNotPresent(&fanins, input_node_name) &&
        can_dedup_control) {
      node->mutable_input()->SwapElements(pos, last_pos);
      --last_pos;
    } else {
      OutputPort output(nodes()[input_node_name], tensor_id.index());

      if (is_control_input) {
        fanouts()[output].emplace(node, Graph::kControlSlot);
      } else {
        max_input_port = pos;
        max_regular_output_port()[output.node] =
            std::max(max_regular_output_port()[output.node], output.port_id);
        fanouts()[output].emplace(node, pos);
      }
      ++pos;
    }
    if (is_control_input) {
      controlling_fanins.insert(input_node_name);
    }
  }

  if (last_pos < last_idx) {
    node->mutable_input()->DeleteSubrange(last_pos + 1, last_idx - last_pos);
  }

  if (max_input_port > -1) {
    max_regular_input_port()[node] = max_input_port;
  }
}

void MutableGraphView::UpdateMaxRegularOutputPortForRemovedFanin(
    const OutputPort& fanin,
    const absl::flat_hash_set<InputPort>& fanin_fanouts) {
  int max_port = max_regular_output_port()[fanin.node];
  if (!fanin_fanouts.empty() || max_port != fanin.port_id) {
    return;
  }
  bool updated_max_port = false;
  for (int i = fanin.port_id - 1; i >= 0; --i) {
    OutputPort fanin_port(fanin.node, i);
    if (!fanouts()[fanin_port].empty()) {
      max_regular_output_port()[fanin.node] = i;
      updated_max_port = true;
      break;
    }
  }
  if (!updated_max_port) {
    max_regular_output_port().erase(fanin.node);
  }
}

void MutableGraphView::UpdateMaxRegularOutputPortForAddedFanin(
    const OutputPort& fanin) {
  if (max_regular_output_port()[fanin.node] < fanin.port_id) {
    max_regular_output_port()[fanin.node] = fanin.port_id;
  }
}

const absl::flat_hash_set<MutableGraphView::InputPort>&
MutableGraphView::GetFanout(const GraphView::OutputPort& port) const {
  return GetFanout(MutableGraphView::OutputPort(const_cast<NodeDef*>(port.node),
                                                port.port_id));
}

absl::flat_hash_set<MutableGraphView::OutputPort> MutableGraphView::GetFanin(
    const GraphView::InputPort& port) const {
  return GetFanin(MutableGraphView::InputPort(const_cast<NodeDef*>(port.node),
                                              port.port_id));
}

const MutableGraphView::OutputPort MutableGraphView::GetRegularFanin(
    const GraphView::InputPort& port) const {
  return GetRegularFanin(MutableGraphView::InputPort(
      const_cast<NodeDef*>(port.node), port.port_id));
}

NodeDef* MutableGraphView::AddNode(NodeDef&& node) {
  auto* node_in_graph = graph()->add_node();
  *node_in_graph = std::move(node);

  AddUniqueNodeOrDie(node_in_graph);

  AddAndDedupFanouts(node_in_graph);
  return node_in_graph;
}

Status MutableGraphView::AddSubgraph(GraphDef&& subgraph) {
  // 1. Add all new functions and check that functions with the same name
  // have identical definition.
  const int function_size = subgraph.library().function_size();
  if (function_size > 0) {
    absl::flat_hash_map<absl::string_view, const FunctionDef*> graph_fdefs;
    for (const FunctionDef& fdef : graph()->library().function()) {
      graph_fdefs.emplace(fdef.signature().name(), &fdef);
    }

    for (FunctionDef& fdef : *subgraph.mutable_library()->mutable_function()) {
      const auto graph_fdef = graph_fdefs.find(fdef.signature().name());

      if (graph_fdef == graph_fdefs.end()) {
        VLOG(3) << "Add new function definition: " << fdef.signature().name();
        graph()->mutable_library()->add_function()->Swap(&fdef);
      } else {
        if (!FunctionDefsEqual(fdef, *graph_fdef->second)) {
          return MutationError(
              "AddSubgraph",
              absl::Substitute("function_size=$0", function_size),
              absl::StrCat(
                  "Found different function definition with the same name: ",
                  fdef.signature().name()));
        }
      }
    }
  }

  // 2. Add all nodes to the underlying graph.
  int node_size_before = graph()->node_size();

  for (NodeDef& node : *subgraph.mutable_node()) {
    auto* node_in_graph = graph()->add_node();
    node_in_graph->Swap(&node);
    TF_RETURN_IF_ERROR(AddUniqueNode(node_in_graph));
  }

  // TODO(ezhulenev, lyandy): Right now AddAndDedupFanouts do not check that
  // fanins actually exists in the graph, and there is already TODO for that.

  for (int i = node_size_before; i < graph()->node_size(); ++i) {
    NodeDef* node = graph()->mutable_node(i);
    AddAndDedupFanouts(node);
  }

  return Status::OK();
}

Status MutableGraphView::UpdateFanouts(absl::string_view from_node_name,
                                       absl::string_view to_node_name) {
  NodeDef* from_node = GetNode(from_node_name);
  TF_RETURN_IF_ERROR(
      CheckNodeExists(from_node_name, from_node,
                      UpdateFanoutsError(from_node_name, to_node_name)));
  NodeDef* to_node = GetNode(to_node_name);
  TF_RETURN_IF_ERROR(CheckNodeExists(
      to_node_name, to_node, UpdateFanoutsError(from_node_name, to_node_name)));

  return UpdateFanoutsInternal(from_node, to_node);
}

Status MutableGraphView::UpdateFanoutsInternal(NodeDef* from_node,
                                               NodeDef* to_node) {
  VLOG(2) << absl::Substitute("Update fanouts from '$0' to '$1'.",
                              from_node->name(), to_node->name());
  if (from_node == to_node) {
    return Status::OK();
  }

  // Update internal state with the new output_port->input_port edge.
  const auto add_edge = [this](const OutputPort& output_port,
                               const InputPort& input_port) {
    fanouts()[output_port].insert(input_port);
  };

  // Remove invalidated edge from the internal state.
  const auto remove_edge = [this](const OutputPort& output_port,
                                  const InputPort& input_port) {
    fanouts()[output_port].erase(input_port);
  };

  // For the control fanouts we do not know the input index in a NodeDef,
  // so we have to traverse all control inputs.

  auto control_fanouts =
      GetFanout(GraphView::OutputPort(from_node, Graph::kControlSlot));

  bool to_node_is_switch = IsSwitch(*to_node);
  for (const InputPort& control_port : control_fanouts) {
    // Node can't be control dependency of itself.
    if (control_port.node == to_node) continue;

    // Can't add Switch node as a control dependency.
    if (to_node_is_switch) {
      // Trying to add a Switch as a control dependency, which if allowed will
      // make the graph invalid.
      return UpdateFanoutsError(from_node->name(), to_node->name())(
          absl::Substitute("can't update fanouts to node '$0' as it will "
                           "become a Switch control dependency",
                           to_node->name()));
    }

    NodeDef* node = control_port.node;
    RemoveControllingFaninInternal(node, from_node);
    AddFaninInternal(node, {to_node, Graph::kControlSlot});
  }

  // First we update regular fanouts. For the regular fanouts
  // `input_port:port_id` is the input index in NodeDef.

  auto regular_edges =
      GetFanoutEdges(*from_node, /*include_controlled_edges=*/false);

  // Maximum index of the `from_node` output tensor that is still used as an
  // input to some other node.
  int keep_max_regular_output_port = -1;

  for (const Edge& edge : regular_edges) {
    const OutputPort output_port = edge.src;
    const InputPort input_port = edge.dst;

    // If the `to_node` reads from the `from_node`, skip this edge (see
    // AddAndUpdateFanoutsWithoutSelfLoops test for an example).
    if (input_port.node == to_node) {
      keep_max_regular_output_port =
          std::max(keep_max_regular_output_port, output_port.port_id);
      continue;
    }

    // Update input at destination node.
    input_port.node->set_input(
        input_port.port_id,
        TensorIdToString({to_node->name(), output_port.port_id}));

    // Remove old edge between the `from_node` and the fanout node.
    remove_edge(output_port, input_port);
    // Add an edge between the `to_node` and new fanout node.
    add_edge(OutputPort(to_node, output_port.port_id), input_port);
    // Dedup control dependency.
    if (CanDedupControlWithRegularInput(*this, *to_node)) {
      RemoveControllingFaninInternal(input_port.node, to_node);
    }
  }

  // Because we update all regular fanouts of `from_node`, we can just copy
  // the value `num_regular_outputs`.
  max_regular_output_port()[to_node] = max_regular_output_port()[from_node];

  // Check if all fanouts were updated to read from the `to_node`.
  if (keep_max_regular_output_port >= 0) {
    max_regular_output_port()[from_node] = keep_max_regular_output_port;
  } else {
    max_regular_output_port().erase(from_node);
  }

  return Status::OK();
}

bool MutableGraphView::AddFaninInternal(NodeDef* node,
                                        const OutputPort& fanin) {
  int num_regular_fanins =
      NumFanins(*node, /*include_controlling_nodes=*/false);
  bool input_is_control = IsOutputPortControlling(fanin);
  bool can_dedup_control_with_regular_input =
      CanDedupControlWithRegularInput(*this, *fanin.node);
  // Don't add duplicate control dependencies.
  if (input_is_control) {
    const int start =
        can_dedup_control_with_regular_input ? 0 : num_regular_fanins;
    for (int i = start; i < node->input_size(); ++i) {
      if (ParseTensorName(node->input(i)).node() == fanin.node->name()) {
        return false;
      }
    }
  }

  InputPort input;
  input.node = node;
  input.port_id = input_is_control ? Graph::kControlSlot : num_regular_fanins;

  node->add_input(TensorIdToString({fanin.node->name(), fanin.port_id}));
  if (!input_is_control) {
    const int last_node_input = node->input_size() - 1;
    // If there are control dependencies in node, move newly inserted fanin to
    // be before such control dependencies.
    if (num_regular_fanins < last_node_input) {
      node->mutable_input()->SwapElements(last_node_input, num_regular_fanins);
    }
  }

  fanouts()[fanin].insert(input);
  if (max_regular_output_port()[fanin.node] < fanin.port_id) {
    max_regular_output_port()[fanin.node] = fanin.port_id;
  }

  // Update max input port and dedup control dependencies.
  if (!input_is_control) {
    max_regular_input_port()[node] = num_regular_fanins;
    if (can_dedup_control_with_regular_input) {
      RemoveControllingFaninInternal(node, fanin.node);
    }
  }

  return true;
}

Status MutableGraphView::AddRegularFanin(absl::string_view node_name,
                                         const TensorId& fanin) {
  auto error_status = [node_name, fanin](absl::string_view msg) {
    string params = absl::Substitute("node_name='$0', fanin='$1'", node_name,
                                     fanin.ToString());
    return MutationError("AddRegularFanin", params, msg);
  };

  TF_RETURN_IF_ERROR(CheckFaninIsRegular(fanin, error_status));
  TF_RETURN_IF_ERROR(CheckAddingFaninToSelf(node_name, fanin, error_status));
  NodeDef* node = GetNode(node_name);
  TF_RETURN_IF_ERROR(CheckNodeExists(node_name, node, error_status));
  NodeDef* fanin_node = GetNode(fanin.node());
  TF_RETURN_IF_ERROR(CheckNodeExists(fanin.node(), fanin_node, error_status));

  AddFaninInternal(node, {fanin_node, fanin.index()});
  return Status::OK();
}

Status MutableGraphView::AddRegularFaninByPort(absl::string_view node_name,
                                               int port,
                                               const TensorId& fanin) {
  auto error_status = [node_name, port, fanin](absl::string_view msg) {
    string params = absl::Substitute("node_name='$0', port=$1, fanin='$2'",
                                     node_name, port, fanin.ToString());
    return MutationError("AddRegularFaninByPort", params, msg);
  };

  TF_RETURN_IF_ERROR(CheckFaninIsRegular(fanin, error_status));
  TF_RETURN_IF_ERROR(CheckAddingFaninToSelf(node_name, fanin, error_status));
  NodeDef* node = GetNode(node_name);
  TF_RETURN_IF_ERROR(CheckNodeExists(node_name, node, error_status));
  const int num_regular_fanins =
      NumFanins(*node, /*include_controlling_nodes=*/false);
  TF_RETURN_IF_ERROR(
      CheckPortRange(port, /*min=*/0, num_regular_fanins, error_status));
  NodeDef* fanin_node = GetNode(fanin.node());
  TF_RETURN_IF_ERROR(CheckNodeExists(fanin.node(), fanin_node, error_status));

  const int last_node_input = node->input_size();
  node->add_input(TensorIdToString(fanin));
  node->mutable_input()->SwapElements(num_regular_fanins, last_node_input);
  for (int i = num_regular_fanins - 1; i >= port; --i) {
    TensorId tensor_id = ParseTensorName(node->input(i));
    OutputPort fanin_port(nodes()[tensor_id.node()], tensor_id.index());
    absl::flat_hash_set<InputPort>* fanouts_set = &fanouts()[fanin_port];
    fanouts_set->erase({node, i});
    fanouts_set->insert({node, i + 1});
    node->mutable_input()->SwapElements(i, i + 1);
  }

  OutputPort fanin_port(fanin_node, fanin.index());
  fanouts()[fanin_port].insert({node, port});
  UpdateMaxRegularOutputPortForAddedFanin(fanin_port);

  max_regular_input_port()[node] = num_regular_fanins;
  if (CanDedupControlWithRegularInput(*this, *fanin_node)) {
    RemoveControllingFaninInternal(node, fanin_node);
  }

  return Status::OK();
}

Status MutableGraphView::AddControllingFanin(absl::string_view node_name,
                                             const TensorId& fanin) {
  auto error_status = [node_name, fanin](absl::string_view msg) {
    string params = absl::Substitute("node_name='$0', fanin='$1'", node_name,
                                     fanin.ToString());
    return MutationError("AddControllingFanin", params, msg);
  };

  TF_RETURN_IF_ERROR(CheckFaninIsValid(fanin, error_status));
  TF_RETURN_IF_ERROR(CheckAddingFaninToSelf(node_name, fanin, error_status));
  NodeDef* node = GetNode(node_name);
  TF_RETURN_IF_ERROR(CheckNodeExists(node_name, node, error_status));
  NodeDef* fanin_node = GetNode(fanin.node());
  TF_RETURN_IF_ERROR(CheckNodeExists(fanin.node(), fanin_node, error_status));

  if (!IsSwitch(*fanin_node)) {
    AddFaninInternal(node, {fanin_node, Graph::kControlSlot});
  } else {
    if (IsTensorIdControlling(fanin)) {
      // Can't add a Switch node control dependency.
      return error_status(absl::Substitute(
          "can't add fanin '$0' as it will become a Switch control dependency",
          fanin.ToString()));
    }
    // We can't anchor control dependencies directly on the switch node: unlike
    // other nodes only one of the outputs of the switch node will be generated
    // when the switch node is executed, and we need to make sure the control
    // dependency is only triggered when the corresponding output is triggered.
    // We start by looking for an identity node connected to the output of the
    // switch node, and use it to anchor the control dependency.
    auto fanouts = GetFanouts(*fanin_node, /*include_controlled_nodes=*/false);
    for (auto fanout : fanouts) {
      if (IsIdentity(*fanout.node) || IsIdentityNSingleInput(*fanout.node)) {
        if (ParseTensorName(fanout.node->input(0)) == fanin) {
          if (fanout.node->name() == node_name) {
            return error_status(
                absl::Substitute("can't add found fanin '$0' to self",
                                 AsControlDependency(fanout.node->name())));
          }
          AddFaninInternal(node, {fanout.node, Graph::kControlSlot});
          return Status::OK();
        }
      }
    }
    // We haven't found an existing node where we can anchor the control
    // dependency: add a new identity node.
    string ctrl_dep_name = AddPrefixToNodeName(
        absl::StrCat(fanin.node(), "_", fanin.index()), kMutableGraphViewCtrl);
    if (node_name == ctrl_dep_name) {
      return error_status(
          absl::Substitute("can't add generated fanin '$0' to self",
                           AsControlDependency(ctrl_dep_name)));
    }

    // Reuse a previously created node, if possible.
    NodeDef* ctrl_dep_node = GetNode(ctrl_dep_name);
    if (ctrl_dep_node == nullptr) {
      NodeDef new_node;
      new_node.set_name(ctrl_dep_name);
      new_node.set_op("Identity");
      new_node.set_device(fanin_node->device());
      (*new_node.mutable_attr())["T"].set_type(
          fanin_node->attr().at("T").type());
      new_node.add_input(TensorIdToString(fanin));
      ctrl_dep_node = AddNode(std::move(new_node));
    }
    AddFaninInternal(node, {ctrl_dep_node, Graph::kControlSlot});
  }
  return Status::OK();
}

bool MutableGraphView::RemoveRegularFaninInternal(NodeDef* node,
                                                  const OutputPort& fanin) {
  auto remove_input = [this, node](const OutputPort& fanin_port,
                                   int node_input_port, bool update_max_port) {
    InputPort input(node, node_input_port);

    absl::flat_hash_set<InputPort>* fanouts_set = &fanouts()[fanin_port];
    fanouts_set->erase(input);
    if (update_max_port) {
      UpdateMaxRegularOutputPortForRemovedFanin(fanin_port, *fanouts_set);
    }
    return fanouts_set;
  };

  auto mutable_inputs = node->mutable_input();
  bool modified = false;
  const int num_regular_fanins =
      NumFanins(*node, /*include_controlling_nodes=*/false);
  int i;
  int curr_pos = 0;
  for (i = 0; i < num_regular_fanins; ++i) {
    TensorId tensor_id = ParseTensorName(node->input(i));
    if (tensor_id.node() == fanin.node->name() &&
        tensor_id.index() == fanin.port_id) {
      remove_input(fanin, i, /*update_max_port=*/true);
      modified = true;
    } else if (modified) {
      // Regular inputs will need to have their ports updated.
      OutputPort fanin_port(nodes()[tensor_id.node()], tensor_id.index());
      auto fanouts_set = remove_input(fanin_port, i, /*update_max_port=*/false);
      fanouts_set->insert({node, curr_pos});
      // Shift inputs to be retained.
      mutable_inputs->SwapElements(i, curr_pos);
      ++curr_pos;
    } else {
      // Skip inputs to be retained until first modification.
      ++curr_pos;
    }
  }

  if (modified) {
    const int last_regular_input_port = curr_pos - 1;
    if (last_regular_input_port < 0) {
      max_regular_input_port().erase(node);
    } else {
      max_regular_input_port()[node] = last_regular_input_port;
    }
    if (curr_pos < i) {
      // Remove fanins from node inputs.
      mutable_inputs->DeleteSubrange(curr_pos, i - curr_pos);
    }
  }

  return modified;
}

Status MutableGraphView::RemoveRegularFanin(absl::string_view node_name,
                                            const TensorId& fanin) {
  auto error_status = [node_name, fanin](absl::string_view msg) {
    string params = absl::Substitute("node_name='$0', fanin='$1'", node_name,
                                     fanin.ToString());
    return MutationError("RemoveRegularFanin", params, msg);
  };

  TF_RETURN_IF_ERROR(CheckFaninIsRegular(fanin, error_status));
  TF_RETURN_IF_ERROR(
      CheckRemovingFaninFromSelf(node_name, fanin, error_status));
  NodeDef* node = GetNode(node_name);
  TF_RETURN_IF_ERROR(CheckNodeExists(node_name, node, error_status));
  NodeDef* fanin_node = GetNode(fanin.node());
  TF_RETURN_IF_ERROR(CheckNodeExists(fanin.node(), fanin_node, error_status));

  RemoveRegularFaninInternal(node, {fanin_node, fanin.index()});
  return Status::OK();
}

Status MutableGraphView::RemoveRegularFaninByPort(absl::string_view node_name,
                                                  int port) {
  auto error_status = [node_name, port](absl::string_view msg) {
    string params =
        absl::Substitute("node_name='$0', port=$1", node_name, port);
    return MutationError("RemoveRegularFaninByPort", params, msg);
  };

  NodeDef* node = GetNode(node_name);
  TF_RETURN_IF_ERROR(CheckNodeExists(node_name, node, error_status));
  const int last_regular_fanin_port =
      gtl::FindWithDefault(max_regular_input_port(), node, -1);
  TF_RETURN_IF_ERROR(
      CheckPortRange(port, /*min=*/0, last_regular_fanin_port, error_status));

  TensorId tensor_id = ParseTensorName(node->input(port));
  OutputPort fanin_port(nodes()[tensor_id.node()], tensor_id.index());
  fanouts()[fanin_port].erase({node, port});
  auto mutable_inputs = node->mutable_input();
  for (int i = port + 1; i <= last_regular_fanin_port; ++i) {
    TensorId tensor_id = ParseTensorName(node->input(i));
    OutputPort fanin_port(nodes()[tensor_id.node()], tensor_id.index());
    absl::flat_hash_set<InputPort>* fanouts_set = &fanouts()[fanin_port];
    fanouts_set->erase({node, i});
    fanouts_set->insert({node, i - 1});
    mutable_inputs->SwapElements(i - 1, i);
  }
  const int last_node_input = node->input_size() - 1;
  if (last_regular_fanin_port < last_node_input) {
    mutable_inputs->SwapElements(last_regular_fanin_port, last_node_input);
  }
  mutable_inputs->RemoveLast();

  const int updated_last_regular_input_port = last_regular_fanin_port - 1;
  if (updated_last_regular_input_port < 0) {
    max_regular_input_port().erase(node);
  } else {
    max_regular_input_port()[node] = updated_last_regular_input_port;
  }

  return Status::OK();
}

bool MutableGraphView::RemoveControllingFaninInternal(NodeDef* node,
                                                      NodeDef* fanin_node) {
  for (int i = node->input_size() - 1; i >= 0; --i) {
    TensorId tensor_id = ParseTensorName(node->input(i));
    if (tensor_id.index() > Graph::kControlSlot) {
      break;
    }
    if (tensor_id.node() == fanin_node->name()) {
      fanouts()[{fanin_node, Graph::kControlSlot}].erase(
          {node, Graph::kControlSlot});
      node->mutable_input()->SwapElements(i, node->input_size() - 1);
      node->mutable_input()->RemoveLast();
      return true;
    }
  }
  return false;
}

Status MutableGraphView::RemoveControllingFanin(
    absl::string_view node_name, absl::string_view fanin_node_name) {
  auto error_status = [node_name, fanin_node_name](absl::string_view msg) {
    string params = absl::Substitute("node_name='$0', fanin_node_name='$1'",
                                     node_name, fanin_node_name);
    return MutationError("RemoveControllingFanin", params, msg);
  };

  TF_RETURN_IF_ERROR(CheckRemovingFaninFromSelf(
      node_name, {fanin_node_name, Graph::kControlSlot}, error_status));
  NodeDef* node = GetNode(node_name);
  TF_RETURN_IF_ERROR(CheckNodeExists(node_name, node, error_status));
  NodeDef* fanin_node = GetNode(fanin_node_name);
  TF_RETURN_IF_ERROR(
      CheckNodeExists(fanin_node_name, fanin_node, error_status));

  RemoveControllingFaninInternal(node, fanin_node);
  return Status::OK();
}

Status MutableGraphView::RemoveAllFanins(absl::string_view node_name,
                                         bool keep_controlling_fanins) {
  NodeDef* node = GetNode(node_name);
  if (node == nullptr) {
    string params =
        absl::Substitute("node_name='$0', keep_controlling_fanins=$1",
                         node_name, keep_controlling_fanins);
    return MutationError("RemoveAllFanins", params,
                         NodeMissingErrorMsg(node_name));
  }

  if (node->input().empty()) {
    return Status::OK();
  }

  const int num_regular_fanins =
      NumFanins(*node, /*include_controlling_nodes=*/false);
  RemoveFaninsInternal(node, keep_controlling_fanins);
  if (keep_controlling_fanins) {
    if (num_regular_fanins == 0) {
      return Status::OK();
    } else if (num_regular_fanins < node->input_size()) {
      node->mutable_input()->DeleteSubrange(0, num_regular_fanins);
    } else {
      node->clear_input();
    }
  } else {
    node->clear_input();
  }
  return Status::OK();
}

Status MutableGraphView::UpdateFanin(absl::string_view node_name,
                                     const TensorId& from_fanin,
                                     const TensorId& to_fanin) {
  auto error_status = [node_name, from_fanin, to_fanin](absl::string_view msg) {
    string params =
        absl::Substitute("node_name='$0', from_fanin='$1', to_fanin='$2'",
                         node_name, from_fanin.ToString(), to_fanin.ToString());
    return MutationError("UpdateFanin", params, msg);
  };

  TF_RETURN_IF_ERROR(CheckFaninIsValid(from_fanin, error_status));
  TF_RETURN_IF_ERROR(CheckFaninIsValid(to_fanin, error_status));
  NodeDef* node = GetNode(node_name);
  TF_RETURN_IF_ERROR(CheckNodeExists(node_name, node, error_status));
  NodeDef* from_fanin_node = GetNode(from_fanin.node());
  TF_RETURN_IF_ERROR(
      CheckNodeExists(from_fanin.node(), from_fanin_node, error_status));
  NodeDef* to_fanin_node = GetNode(to_fanin.node());
  TF_RETURN_IF_ERROR(
      CheckNodeExists(to_fanin.node(), to_fanin_node, error_status));

  // When replacing a non control dependency fanin with a control dependency, or
  // vice versa, remove and add, so ports can be updated properly in fanout(s).
  bool to_fanin_is_control = IsTensorIdControlling(to_fanin);
  if (to_fanin_is_control && IsSwitch(*to_fanin_node)) {
    // Can't add Switch node as a control dependency.
    return error_status(
        absl::Substitute("can't update to fanin '$0' as it will become a "
                         "Switch control dependency",
                         to_fanin.ToString()));
  }
  if (node_name == from_fanin.node() || node_name == to_fanin.node()) {
    return error_status("can't update fanin to or from self");
  }

  if (from_fanin == to_fanin) {
    return Status::OK();
  }

  bool from_fanin_is_control = IsTensorIdControlling(from_fanin);
  if (from_fanin_is_control || to_fanin_is_control) {
    bool modified = false;
    if (from_fanin_is_control) {
      modified |= RemoveControllingFaninInternal(node, from_fanin_node);
    } else {
      modified |= RemoveRegularFaninInternal(
          node, {from_fanin_node, from_fanin.index()});
    }
    if (modified) {
      AddFaninInternal(node, {to_fanin_node, to_fanin.index()});
    }
    return Status::OK();
  }

  // In place mutation of regular fanins, requires no shifting of ports.
  string to_fanin_string = TensorIdToString(to_fanin);
  const int num_regular_fanins =
      NumFanins(*node, /*include_controlling_nodes=*/false);
  bool modified = false;
  absl::flat_hash_set<InputPort>* from_fanin_port_fanouts = nullptr;
  absl::flat_hash_set<InputPort>* to_fanin_port_fanouts = nullptr;
  for (int i = 0; i < num_regular_fanins; ++i) {
    if (ParseTensorName(node->input(i)) == from_fanin) {
      InputPort input(node, i);
      if (from_fanin_port_fanouts == nullptr) {
        OutputPort from_fanin_port(from_fanin_node, from_fanin.index());
        from_fanin_port_fanouts = &fanouts()[from_fanin_port];
      }
      from_fanin_port_fanouts->erase(input);

      if (to_fanin_port_fanouts == nullptr) {
        OutputPort to_fanin_port(to_fanin_node, to_fanin.index());
        to_fanin_port_fanouts = &fanouts()[to_fanin_port];
      }
      to_fanin_port_fanouts->insert(input);

      node->set_input(i, to_fanin_string);
      modified = true;
    }
  }

  // Dedup control dependencies and update max regular output ports.
  if (modified) {
    UpdateMaxRegularOutputPortForRemovedFanin(
        {from_fanin_node, from_fanin.index()}, *from_fanin_port_fanouts);
    if (max_regular_output_port()[to_fanin_node] < to_fanin.index()) {
      max_regular_output_port()[to_fanin_node] = to_fanin.index();
    }
    if (CanDedupControlWithRegularInput(*this, *to_fanin_node)) {
      RemoveControllingFaninInternal(node, to_fanin_node);
    }
  }

  return Status::OK();
}

Status MutableGraphView::UpdateRegularFaninByPort(absl::string_view node_name,
                                                  int port,
                                                  const TensorId& fanin) {
  auto error_status = [node_name, port, fanin](absl::string_view msg) {
    string params = absl::Substitute("node_name='$0', port=$1, fanin='$2'",
                                     node_name, port, fanin.ToString());
    return MutationError("UpdateRegularFaninByPort", params, msg);
  };

  TF_RETURN_IF_ERROR(CheckFaninIsRegular(fanin, error_status));
  TF_RETURN_IF_ERROR(CheckAddingFaninToSelf(node_name, fanin, error_status));
  NodeDef* node = GetNode(node_name);
  TF_RETURN_IF_ERROR(CheckNodeExists(node_name, node, error_status));
  const int last_regular_fanin_port =
      gtl::FindWithDefault(max_regular_input_port(), node, -1);
  TF_RETURN_IF_ERROR(
      CheckPortRange(port, /*min=*/0, last_regular_fanin_port, error_status));
  NodeDef* fanin_node = GetNode(fanin.node());
  TF_RETURN_IF_ERROR(CheckNodeExists(fanin.node(), fanin_node, error_status));

  TensorId tensor_id = ParseTensorName(node->input(port));
  if (tensor_id == fanin) {
    return Status::OK();
  }

  InputPort input(node, port);
  OutputPort from_fanin_port(nodes()[tensor_id.node()], tensor_id.index());
  absl::flat_hash_set<InputPort>* from_fanouts = &fanouts()[from_fanin_port];
  from_fanouts->erase(input);
  UpdateMaxRegularOutputPortForRemovedFanin(from_fanin_port, *from_fanouts);

  OutputPort to_fanin_port(fanin_node, fanin.index());
  fanouts()[to_fanin_port].insert(input);
  UpdateMaxRegularOutputPortForAddedFanin(to_fanin_port);

  node->set_input(port, TensorIdToString(fanin));

  if (CanDedupControlWithRegularInput(*this, *fanin_node)) {
    RemoveControllingFaninInternal(node, fanin_node);
  }

  return Status::OK();
}

Status MutableGraphView::SwapRegularFaninsByPorts(absl::string_view node_name,
                                                  int from_port, int to_port) {
  auto error_status = [node_name, from_port, to_port](absl::string_view msg) {
    string params = absl::Substitute("node_name='$0', from_port=$1, to_port=$2",
                                     node_name, from_port, to_port);
    return MutationError("SwapRegularFaninsByPorts", params, msg);
  };

  NodeDef* node = GetNode(node_name);
  TF_RETURN_IF_ERROR(CheckNodeExists(node_name, node, error_status));
  const int last_regular_fanin_port =
      gtl::FindWithDefault(max_regular_input_port(), node, -1);
  TF_RETURN_IF_ERROR(CheckPortRange(from_port, /*min=*/0,
                                    last_regular_fanin_port, error_status));
  TF_RETURN_IF_ERROR(CheckPortRange(to_port, /*min=*/0, last_regular_fanin_port,
                                    error_status));

  if (from_port == to_port) {
    return Status::OK();
  }
  TensorId from_fanin = ParseTensorName(node->input(from_port));
  TensorId to_fanin = ParseTensorName(node->input(to_port));
  if (from_fanin == to_fanin) {
    return Status::OK();
  }

  InputPort from_input(node, from_port);
  InputPort to_input(node, to_port);
  NodeDef* from_fanin_node = GetNode(from_fanin.node());
  absl::flat_hash_set<InputPort>* from_fanouts =
      &fanouts()[{from_fanin_node, from_fanin.index()}];
  from_fanouts->erase(from_input);
  from_fanouts->insert(to_input);
  NodeDef* to_fanin_node = GetNode(to_fanin.node());
  absl::flat_hash_set<InputPort>* to_fanouts =
      &fanouts()[{to_fanin_node, to_fanin.index()}];
  to_fanouts->erase(to_input);
  to_fanouts->insert(from_input);

  node->mutable_input()->SwapElements(from_port, to_port);

  return Status::OK();
}

Status MutableGraphView::CheckNodesCanBeDeleted(
    const absl::flat_hash_set<string>& nodes_to_delete) {
  std::vector<string> missing_nodes;
  std::vector<string> nodes_with_fanouts;
  for (const string& node_name_to_delete : nodes_to_delete) {
    NodeDef* node = GetNode(node_name_to_delete);
    if (node == nullptr) {
      // Can't delete missing node.
      missing_nodes.push_back(node_name_to_delete);
      continue;
    }
    const int max_port = gtl::FindWithDefault(max_regular_output_port(), node,
                                              Graph::kControlSlot);
    for (int i = Graph::kControlSlot; i <= max_port; ++i) {
      auto it = fanouts().find({node, i});
      bool has_retained_fanout = false;
      if (it != fanouts().end()) {
        for (const auto& fanout : it->second) {
          // Check if fanouts are of nodes to be deleted, and if so, they can be
          // ignored, as they will be removed also.
          if (!nodes_to_delete.contains(fanout.node->name())) {
            // Removing node will leave graph in an invalid state.
            has_retained_fanout = true;
            break;
          }
        }
      }
      if (has_retained_fanout) {
        nodes_with_fanouts.push_back(node_name_to_delete);
        break;
      }
    }
  }

  // Error message can get quite long, so we only show the first 5 node names.
  auto sort_and_sample = [](std::vector<string>* s) {
    constexpr int kMaxNodeNames = 5;
    std::sort(s->begin(), s->end());
    if (s->size() > kMaxNodeNames) {
      return absl::StrCat(
          absl::StrJoin(s->begin(), s->begin() + kMaxNodeNames, ", "), ", ...");
    }
    return absl::StrJoin(*s, ", ");
  };

  if (!missing_nodes.empty()) {
    VLOG(2) << absl::Substitute("Attempting to delete missing node(s) [$0].",
                                sort_and_sample(&missing_nodes));
  }
  if (!nodes_with_fanouts.empty()) {
    std::vector<string> input_node_names(nodes_to_delete.begin(),
                                         nodes_to_delete.end());
    string params = absl::Substitute("nodes_to_delete={$0}",
                                     sort_and_sample(&input_node_names));
    string error_msg =
        absl::Substitute("can't delete node(s) with retained fanouts(s) [$0]",
                         sort_and_sample(&nodes_with_fanouts));
    return MutationError("DeleteNodes", params, error_msg);
  }

  return Status::OK();
}

Status MutableGraphView::DeleteNodes(
    const absl::flat_hash_set<string>& nodes_to_delete) {
  TF_RETURN_IF_ERROR(CheckNodesCanBeDeleted(nodes_to_delete));

  // Find nodes in internal state and delete.
  for (const string& node_name_to_delete : nodes_to_delete) {
    NodeDef* node = GetNode(node_name_to_delete);
    if (node != nullptr) {
      RemoveFaninsInternal(node, /*keep_controlling_fanins=*/false);
      RemoveFanoutsInternal(node);
    }
  }
  for (const string& node_name_to_delete : nodes_to_delete) {
    nodes().erase(node_name_to_delete);
  }

  // Find nodes in graph and delete by partitioning into nodes to retain and
  // nodes to delete based on input set of nodes to delete by name.
  // TODO(lyandy): Use a node name->idx hashmap if this is a performance
  // bottleneck.
  int pos = 0;
  const int last_idx = graph()->node_size() - 1;
  int last_pos = last_idx;
  while (pos <= last_pos) {
    if (nodes_to_delete.contains(graph()->node(pos).name())) {
      graph()->mutable_node()->SwapElements(pos, last_pos);
      --last_pos;
    } else {
      ++pos;
    }
  }
  if (last_pos < last_idx) {
    graph()->mutable_node()->DeleteSubrange(last_pos + 1, last_idx - last_pos);
  }

  return Status::OK();
}

void MutableGraphView::RemoveFaninsInternal(NodeDef* deleted_node,
                                            bool keep_controlling_fanins) {
  for (int i = 0; i < deleted_node->input_size(); ++i) {
    TensorId tensor_id = ParseTensorName(deleted_node->input(i));
    bool is_control = IsTensorIdControlling(tensor_id);
    if (keep_controlling_fanins && is_control) {
      break;
    }
    OutputPort fanin(nodes()[tensor_id.node()], tensor_id.index());

    InputPort input;
    input.node = deleted_node;
    input.port_id = is_control ? Graph::kControlSlot : i;

    auto it = fanouts().find(fanin);
    if (it != fanouts().end()) {
      absl::flat_hash_set<InputPort>* fanouts_set = &it->second;
      fanouts_set->erase(input);
      UpdateMaxRegularOutputPortForRemovedFanin(fanin, *fanouts_set);
    }
  }
  max_regular_input_port().erase(deleted_node);
}

void MutableGraphView::RemoveFanoutsInternal(NodeDef* deleted_node) {
  const int max_port =
      gtl::FindWithDefault(max_regular_output_port(), deleted_node, -1);
  for (int i = Graph::kControlSlot; i <= max_port; ++i) {
    fanouts().erase({deleted_node, i});
  }
  max_regular_output_port().erase(deleted_node);
}

}  // end namespace grappler
}  // end namespace tensorflow
