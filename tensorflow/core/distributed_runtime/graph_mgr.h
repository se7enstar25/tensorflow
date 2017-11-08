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

#ifndef TENSORFLOW_CORE_DISTRIBUTED_RUNTIME_GRAPH_MGR_H_
#define TENSORFLOW_CORE_DISTRIBUTED_RUNTIME_GRAPH_MGR_H_

#include <unordered_map>
#include <vector>

#include "tensorflow/core/common_runtime/costmodel_manager.h"
#include "tensorflow/core/common_runtime/executor.h"
#include "tensorflow/core/common_runtime/process_function_library_runtime.h"
#include "tensorflow/core/distributed_runtime/message_wrappers.h"
#include "tensorflow/core/distributed_runtime/worker_env.h"
#include "tensorflow/core/framework/cancellation.h"
#include "tensorflow/core/framework/cost_graph.pb.h"
#include "tensorflow/core/framework/function.h"
#include "tensorflow/core/lib/core/refcount.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/macros.h"
#include "tensorflow/core/platform/mutex.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/protobuf/config.pb.h"
#include "tensorflow/core/protobuf/debug.pb.h"
#include "tensorflow/core/protobuf/worker.pb.h"

namespace tensorflow {

class ExecutorOpts;
class StepStatsCollector;
class RendezvousMgrInterface;
class DeviceMgr;
struct WorkerSession;

// GraphMgr keeps track of a set of graphs that are registered with a
// TensorFlow worker. Each registered graph is identified by a handle
// that is generated by GraphMgr and returned to the caller.
//
// After a successful registration, the caller executes a graph using
// the graph handle. Each execution is distinguished from others by a
// caller generated global unique id "step_id". Multiple executions
// can use the same graph concurrently and independently as long as
// "step_id" used are different.
//
// Multiple threads can call GraphMgr methods concurrently.
//
// E.g.,
//   GraphMgr gmgr(worker_env);
//   string handle;
//   TF_CHECK_OK(gmgr.Register("session", { graph computes c = a + b },
//   &handle));
//   GraphMgr::NamedTensors in = { { "a", Tensor({1, 2}) },
//                                { "b", Tensor({3, 4}) } };
//   GraphMgr::NamedTensors out = { { "c", Tensor() } };
//   TF_CHECK_OK(gmgr.Execute(handle, 0x0001, in, &out));
//   EXPECT_EQ(out["c"], Tensor({4, 6}));
class GraphMgr {
 public:
  explicit GraphMgr(const WorkerEnv* worker_env, DeviceMgr* device_mgr);
  ~GraphMgr();

  // Registers a graph. Fills in "handle". The registered graph retains a
  // reference to cluster_flr to do cross process function calls.
  Status Register(const string& session, const GraphDef& gdef,
                  const GraphOptions& graph_options,
                  const DebugOptions& debug_options,
                  DistributedFunctionLibraryRuntime* cluster_flr,
                  string* handle);

  // Executes one step of a registered graph "handle".
  //
  // If "out" is not nullptr, "out" specifies all keys the execution
  // should receive upon finish.
  typedef std::map<string, Tensor> NamedTensors;
  typedef std::function<void(const Status&)> StatusCallback;
  void ExecuteAsync(const string& handle, const int64 step_id,
                    WorkerSession* session, const ExecutorOpts& opts,
                    StepStatsCollector* collector,
                    MutableRunGraphResponseWrapper* response,
                    CancellationManager* cancellation_manager,
                    const NamedTensors& in, StatusCallback done);

  Status SendInputs(const int64 step_id, const NamedTensors& in);
  Status RecvOutputs(const int64 step_id, NamedTensors* out);
  void RecvOutputsAsync(const int64 step_id, NamedTensors* out,
                        StatusCallback done);

  // Deregisters a graph.
  Status Deregister(const string& handle);

  // Deregister all graphs.
  Status DeregisterAll();

 private:
  typedef GraphMgr ME;

  struct ExecutionUnit {
    Graph* graph = nullptr;                 // not owned.
    Device* device = nullptr;               // not owned.
    Executor* root = nullptr;               // not owned.
    FunctionLibraryRuntime* lib = nullptr;  // not owned.
    // Build the cost model if this value is strictly positive.
    int64 build_cost_model = 0;
  };

  struct Item : public core::RefCounted {
    // TODO(zhifengc): Keeps a copy of the original graph if the need arises.
    // TODO(zhifengc): Stats, updated by multiple runs potentially.
    // TODO(zhifengc): Dup-detection. Ensure step_id only run once.
    ~Item() override;

    // Session handle.
    string session;

    // Graph handle.
    string handle;

    std::unique_ptr<FunctionLibraryDefinition> lib_def;
    // Owns the FunctionLibraryRuntime objects needed to execute functions, one
    // per device.
    std::unique_ptr<ProcessFunctionLibraryRuntime> proc_flr;
    // A graph is partitioned over multiple devices.  Each partition
    // has a root executor which may call into the runtime library.
    std::vector<ExecutionUnit> units;

    // Used to deregister a cost model when cost model is required in graph
    // manager.
    GraphMgr* graph_mgr;
  };

  const WorkerEnv* worker_env_;             // Not owned.
  DeviceMgr* device_mgr_;

  CostModelManager cost_model_manager_;

  // Owned.
  mutex mu_;
  int64 next_id_ GUARDED_BY(mu_) = 0;

  // If true, blocks until device has finished all queued operations in a step.
  bool sync_on_finish_ = true;

  // Table mapping graph handles to registered graphs.
  //
  // TODO(zhifengc): If the client does not call Deregister, we'll
  // lose memory over time. We should implement a timeout-based
  // mechanism to gc these graphs.
  std::unordered_map<string, Item*> table_;

  void StartParallelExecutors(const string& handle, int64 step_id, Item* item,
                              Rendezvous* rendezvous,
                              StepStatsCollector* collector,
                              CostGraphDef* cost_graph,
                              CancellationManager* cancellation_manager,
                              StatusCallback done);

  // Don't attempt to process cost models unless explicitly requested for at
  // least one of the items.
  bool skip_cost_models_ = true;

  void BuildCostModel(Item* item, StepStatsCollector* collector,
                      CostGraphDef* cost_graph);

  Status InitItem(const string& session, const GraphDef& gdef,
                  const GraphOptions& graph_options,
                  const DebugOptions& debug_options,
                  DistributedFunctionLibraryRuntime* cluster_flr, Item* item);

  Status DecorateAndPublishGraphForDebug(const DebugOptions& debug_options,
                                         Graph* graph, Device* device);

  TF_DISALLOW_COPY_AND_ASSIGN(GraphMgr);
};

}  // end namespace tensorflow

#endif  // TENSORFLOW_CORE_DISTRIBUTED_RUNTIME_GRAPH_MGR_H_
