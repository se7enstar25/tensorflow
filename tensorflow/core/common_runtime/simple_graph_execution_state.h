/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_CORE_COMMON_RUNTIME_SIMPLE_GRAPH_EXECUTION_STATE_H_
#define TENSORFLOW_CORE_COMMON_RUNTIME_SIMPLE_GRAPH_EXECUTION_STATE_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "tensorflow/core/common_runtime/build_graph_options.h"
#include "tensorflow/core/common_runtime/device.h"
#include "tensorflow/core/common_runtime/device_set.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/framework/step_stats.pb.h"
#include "tensorflow/core/graph/costmodel.h"
#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/platform/macros.h"
#include "tensorflow/core/platform/mutex.h"
#include "tensorflow/core/platform/thread_annotations.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {
struct SessionOptions;
class StepStats;
class Timeline;

struct SimpleGraphExecutionStateOptions {
  const DeviceSet* device_set = nullptr;
  const SessionOptions* session_options = nullptr;
  // A map from node name to device name, representing the unchangeable
  // placement of stateful nodes.
  std::unordered_map<string, string> stateful_placements;
};

// A SimpleClientGraph is simply a sub-graph of the full graph as induced by
// BuildGraphOptions.
struct SimpleClientGraph {
  explicit SimpleClientGraph(std::unique_ptr<FunctionLibraryDefinition> flib)
      : flib_def(std::move(flib)), graph(flib_def.get()) {}
  // Each client-graph gets its own function library since optimization passes
  // post rewrite for execution might want to introduce new functions.
  std::unique_ptr<FunctionLibraryDefinition> flib_def;
  Graph graph;
  int32 placement_version;
};

// SimpleGraphExecutionState is responsible for generating an
// executable SimpleClientGraph from the original GraphDef that specifies
// the complete graph and from BuildGraphOptions which specifies
// input/output nodes.
//
// An executable Graph differs from a GraphDef by being Placed,
// meaning that each Node is assigned to a single Device in the
// available set.
//
// When SimpleGraphExecutionState is first constructed it instantiates
// a full Graph from the provided GraphDef, and places it, using only
// the static device assignments from the GraphDef.  Nodes without are
// currently placed in a very naive way.  Since stateful Nodes cannot
// be moved after initial placement, it is important that stateful
// Nodes get sensible initial device assignments in the graph
// definition.
//
// Subsequently, SimpleGraphExecutionState generates a SimpleClientGraph on
// demand, which is a sub-graph of the latest placement of the full
// Graph.  MasterSession uses such a SimpleClientGraph to execute one or
// more similar client requests.
//
// SimpleGraphExecutionState is thread-safe.

class SimpleGraphExecutionState {
 public:
  virtual ~SimpleGraphExecutionState();

  // Creates a new `SimpleGraphExecutionState` for the given
  // `graph_def`, which represents the entire graph for a session.
  //
  // N.B. This method uses `GraphDef::Swap()` and leaves `graph_def`
  // in an undefined state. If it is necessary to use `*graph_def`
  // after this call, make an explicit copy of the graph before
  // calling this method.
  static Status MakeForBaseGraph(
      GraphDef* graph_def, const SimpleGraphExecutionStateOptions& options,
      std::unique_ptr<SimpleGraphExecutionState>* out_state);

  // Creates a new `SimpleGraphExecutionState` and `SimpleClientGraph`
  // for the subgraph of `original_graph_def` defined by
  // `subgraph_options`.
  static Status MakeForPrunedGraph(
      const FunctionDefLibrary& func_def_lib,
      const SimpleGraphExecutionStateOptions& options,
      const GraphDef& original_graph_def,
      const BuildGraphOptions& subgraph_options,
      std::unique_ptr<SimpleGraphExecutionState>* out_state,
      std::unique_ptr<SimpleClientGraph>* out_client_graph);

  // Creates a new SimpleGraphExecutionState representing the
  // concatenation of this graph, and the graph defined by
  // "extension_def". The same name may not be used to define a node
  // in both this graph and "extension_def".
  //
  // If successful, returns OK and the caller takes ownership of "*out".
  // Otherwise returns an error and does not modify "*out".
  //
  // After calling `old_state->Extend()`, `old_state` may no longer be
  // used.
  //
  // NOTE(mrry): This method respects the placement of stateful nodes in
  // in *this, but currently does not transfer any other placement
  // or cost model information to the new graph.
  Status Extend(const GraphDef& extension_def,
                std::unique_ptr<SimpleGraphExecutionState>* out) const;

  // Builds a SimpleClientGraph (a sub-graph of the full graph as induced by
  // the Node set specified in "options").  If successful, returns OK
  // and the caller takes the ownership of "*out". Otherwise, returns
  // an error.
  Status BuildGraph(const BuildGraphOptions& options,
                    std::unique_ptr<SimpleClientGraph>* out);

  // Sums execution statistics in "ss" into the CostModel.
  void UpdateCostsFromStats(const StepStats& ss);

  Microseconds TimeEstimate(const Node* n) {
    mutex_lock l(mu_);  // could use reader lock
    return costs_.TimeEstimate(n);
  }

  Bytes SizeEstimate(const Node* n, int output_slot) {
    mutex_lock l(mu_);  // could use reader lock
    return costs_.SizeEstimate(n, output_slot);
  }

  // Merge the cost model maintained by this graph_execution_state to 'costs'.
  void MergeCostsFromGlobal(CostModel* costs);

  // The graph returned by BuildGraph may contain only the pruned
  // graph, whereas some clients may want access to the full graph.
  const Graph* full_graph() {
    return graph_;
  }

  // Returns the node with the given name, or null if it does not exist.
  const Node* get_node_by_name(const string& name) const {
    NodeNameToCostIdMap::const_iterator iter =
        node_name_to_cost_id_map_.find(name);
    if (iter != node_name_to_cost_id_map_.end()) {
      return graph_->FindNodeId(iter->second);
    } else {
      return nullptr;
    }
  }

  // Returns a reference to the current graph_def.  Use must
  // not extend beyond lifetime of SimpleGrahExecutionState object.
  const GraphDef& original_graph_def() { return original_graph_def_; }

  // Returns the map of stateful placements as a map of
  // node name to placement string.
  std::unordered_map<string, string> GetStatefulPlacements() const {
    mutex_lock l(mu_);
    return stateful_placements_;
  }

 private:
  SimpleGraphExecutionState(GraphDef* graph_def,
                            const SimpleGraphExecutionStateOptions& options);

  Status InitBaseGraph(const BuildGraphOptions& options);

  // Map of placed stateful nodes, i.e. nodes for which is_stateful()
  // is true, such as "params" and "queue" nodes.  Once placed these
  // nodes can not be moved to a different device.  Maps node names to
  // device names.
  std::unordered_map<string, string> stateful_placements_;  // Immutable after
                                                            // ctor.
  void SaveStatefulNodes(Graph* graph);
  void RestoreStatefulNodes(Graph* graph);

  GraphDef original_graph_def_;            // Immutable after ctor.
  const DeviceSet* device_set_;            // Not owned
  const SessionOptions* session_options_;  // Not owned

  mutable mutex mu_;
  CostModel costs_ GUARDED_BY(mu_);

  // Map from name to Node for the full graph in placed_.
  NodeNameToCostIdMap node_name_to_cost_id_map_;

  // 'flib_def_' is initialized from the initial graph def's library,
  // and may be updated by a graph optimization pass.
  std::unique_ptr<FunctionLibraryDefinition> flib_def_;

  // The dataflow graph owned by this object.
  Graph* graph_;

  TF_DISALLOW_COPY_AND_ASSIGN(SimpleGraphExecutionState);
};

}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_COMMON_RUNTIME_SIMPLE_GRAPH_EXECUTION_STATE_H_
