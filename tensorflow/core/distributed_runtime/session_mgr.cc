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

#include "tensorflow/core/distributed_runtime/session_mgr.h"

#include <utility>

#include "tensorflow/core/common_runtime/device_mgr.h"
#include "tensorflow/core/common_runtime/renamed_device.h"
#include "tensorflow/core/distributed_runtime/graph_mgr.h"
#include "tensorflow/core/distributed_runtime/remote_device.h"
#include "tensorflow/core/distributed_runtime/worker_cache_wrapper.h"
#include "tensorflow/core/lib/strings/strcat.h"
#include "tensorflow/core/protobuf/cluster.pb.h"
#include "tensorflow/core/protobuf/tensorflow_server.pb.h"
#include "tensorflow/core/util/ptr_util.h"

namespace tensorflow {

SessionMgr::SessionMgr(
    WorkerEnv* worker_env, const string& default_worker_name,
    std::unique_ptr<WorkerCacheInterface> default_worker_cache,
    WorkerCacheFactory worker_cache_factory)
    : worker_env_(worker_env),
      default_worker_cache_(std::move(default_worker_cache)),
      legacy_session_(WorkerSession::CreateWithBorrowedDeviceMgr(
          "", default_worker_name,
          std::unique_ptr<WorkerCacheInterface>(
              new WorkerCacheWrapper(default_worker_cache_.get())),
          worker_env->device_mgr,
          std::unique_ptr<GraphMgr>(
              new GraphMgr(worker_env, worker_env->device_mgr)),
          nullptr)),
      worker_cache_factory_(std::move(worker_cache_factory)) {}

/* static */
string SessionMgr::WorkerNameFromServerDef(const ServerDef& server_def) {
  return strings::StrCat("/job:", server_def.job_name(),
                         "/replica:0/task:", server_def.task_index());
}

Status SessionMgr::CreateSession(const string& session,
                                 const ServerDef& server_def,
                                 bool isolate_session_state) {
  return CreateSession(session, server_def, {}, isolate_session_state);
}

Status SessionMgr::CreateSession(
    const string& session, const ServerDef& server_def,
    const protobuf::RepeatedPtrField<DeviceAttributes>&
        cluster_device_attributes,
    bool isolate_session_state) {
  mutex_lock l(mu_);
  if (session.empty()) {
    return errors::InvalidArgument("Session must be non-empty.");
  }

  WorkerCacheInterface* worker_cache = nullptr;
  string worker_name;
  if (server_def.cluster().job().empty()) {
    worker_cache = new WorkerCacheWrapper(default_worker_cache_.get());
    worker_name = legacy_session_->worker_name();
  } else {
    TF_RETURN_IF_ERROR(worker_cache_factory_(server_def, &worker_cache));
    worker_name = WorkerNameFromServerDef(server_def);
  }

  if (worker_cache != nullptr && default_worker_cache_ != nullptr) {
    worker_cache->SetLogging(this->is_logging_active_);
  }

  CHECK(!worker_env_->local_devices.empty())
      << "The WorkerEnv must have at least one device in `local_devices`.";

  std::shared_ptr<WorkerSession> worker_session;
  std::vector<std::unique_ptr<Device>> cluster_devices;

  if (isolate_session_state || server_def.cluster().job_size()) {
    if (server_def.cluster().job_size()) {
      VLOG(1) << "ClusterSpec propagation is enabled.";
    }
    if (!isolate_session_state) {
      VLOG(1) << "Session state isolation is disabled.";
    }

    // Create a private copy of the DeviceMgr for the WorkerSession.
    std::vector<std::unique_ptr<Device>> renamed_devices;
    for (Device* d : worker_env_->local_devices) {
      renamed_devices.push_back(RenamedDevice::NewRenamedDevice(
          worker_name, d, false, isolate_session_state));
    }
    auto device_mgr = MakeUnique<StaticDeviceMgr>(std::move(renamed_devices));
    LookupLocalDevice cb = [&device_mgr](StringPiece name, Device** device) {
      return device_mgr->LookupDevice(name, device);
    };
    AsRemoteDevices(worker_env_->env, cluster_device_attributes, cb,
                    &cluster_devices);
    std::unique_ptr<DynamicDeviceMgr> remote_devices;
    if (!cluster_device_attributes.empty()) {
      remote_devices = MakeUnique<DynamicDeviceMgr>();
      TF_RETURN_IF_ERROR(
          remote_devices->AddDevices(std::move(cluster_devices)));
    }

    auto graph_mgr = MakeUnique<GraphMgr>(worker_env_, device_mgr.get());
    worker_session.reset(
        new WorkerSession(session, worker_name,
                          std::unique_ptr<WorkerCacheInterface>(worker_cache),
                          std::move(device_mgr), std::move(graph_mgr),
                          std::move(remote_devices)));
  } else {
    AsRemoteDevices(worker_env_->env, cluster_device_attributes, nullptr,
                    &cluster_devices);
    std::unique_ptr<DynamicDeviceMgr> remote_devices;
    if (!cluster_device_attributes.empty()) {
      remote_devices = MakeUnique<DynamicDeviceMgr>();
      TF_RETURN_IF_ERROR(
          remote_devices->AddDevices(std::move(cluster_devices)));
    }
    // Borrow the WorkerEnv's DeviceMgr for the WorkerSession, so
    // that resources using it can use its devices after the
    // WorkerSession has been deleted.
    auto graph_mgr = MakeUnique<GraphMgr>(worker_env_, worker_env_->device_mgr);
    worker_session = WorkerSession::CreateWithBorrowedDeviceMgr(
        session, worker_name,
        std::unique_ptr<WorkerCacheInterface>(worker_cache),
        worker_env_->device_mgr, std::move(graph_mgr),
        std::move(remote_devices));
  }

  sessions_.insert(std::make_pair(session, std::move(worker_session)));
  return Status::OK();
}

Status SessionMgr::UpdateSession(
    const string& session, const ServerDef& server_def,
    const protobuf::RepeatedPtrField<DeviceAttributes>&
        cluster_device_attributes,
    bool isolate_session_state) {
  mutex_lock l(mu_);
  if (session.empty()) {
    return errors::InvalidArgument("Session must be non-empty.");
  }
  auto it = sessions_.find(session);
  if (it == sessions_.end()) {
    return errors::InvalidArgument("Cannot update session ", session,
                                   " because it does not exist.");
  }
  std::shared_ptr<WorkerSession> worker_session = it->second;

  WorkerCacheInterface* worker_cache = nullptr;
  if (server_def.cluster().job().empty()) {
    worker_cache = new WorkerCacheWrapper(default_worker_cache_.get());
  } else {
    TF_RETURN_IF_ERROR(worker_cache_factory_(server_def, &worker_cache));
  }
  std::vector<string> updated_remote_workers;
  worker_cache->ListWorkers(&updated_remote_workers);

  std::vector<std::unique_ptr<Device>> cluster_devices;

  DeviceMgr* local_device_mgr = worker_session->device_mgr();
  DeviceMgr* remote_device_mgr = worker_session->remote_device_mgr();
  std::vector<Device*> curr_remote_devices = remote_device_mgr->ListDevices();
  std::vector<std::unique_ptr<Device>> added_remote_devices;
  std::vector<Device*> removed_remote_devices;

  std::vector<DeviceAttributes> added_cluster_device_attrs;
  for (const auto& da : cluster_device_attributes) {
    Device* device;
    if (!local_device_mgr->LookupDevice(da.name(), &device).ok() &&
        !remote_device_mgr->LookupDevice(da.name(), &device).ok()) {
      added_cluster_device_attrs.emplace_back(da);
    } else if (device != nullptr &&
               device->attributes().incarnation() != da.incarnation()) {
      removed_remote_devices.emplace_back(device);
      added_cluster_device_attrs.emplace_back(da);
    }
  }
  for (Device* device : curr_remote_devices) {
    string task_name;
    DeviceNameUtils::GetTaskName(device->parsed_name(), &task_name);
    if (std::find(updated_remote_workers.begin(), updated_remote_workers.end(),
                  task_name) == updated_remote_workers.end()) {
      removed_remote_devices.emplace_back(device);
    }
  }
  protobuf::RepeatedPtrField<DeviceAttributes> added_cluster_device_attrs_pb(
      added_cluster_device_attrs.begin(), added_cluster_device_attrs.end());
  std::unique_ptr<DeviceMgr> remote_devices;
  AsRemoteDevices(worker_env_->env, added_cluster_device_attrs_pb, nullptr,
                  &added_remote_devices);

  TF_RETURN_IF_ERROR(worker_session->UpdateWorkerCacheAndDevices(
      std::unique_ptr<WorkerCacheInterface>(worker_cache),
      std::move(added_remote_devices), removed_remote_devices));
  return Status::OK();
}

Status SessionMgr::DeleteSession(const string& session) {
  mutex_lock l(mu_);
  auto it = sessions_.find(session);
  if (it != sessions_.end()) {
    sessions_.erase(it);
  }
  return Status::OK();
}

Status SessionMgr::WorkerSessionForSessionLocked(
    const string& session_handle, std::shared_ptr<WorkerSession>* out_session) {
  if (session_handle.empty()) {
    *out_session = legacy_session_;
  } else {
    auto it = sessions_.find(session_handle);
    if (it == sessions_.end()) {
      return errors::Aborted("Session handle is not found: ", session_handle,
                             ". Possibly this worker (\"",
                             legacy_session_->worker_name(),
                             "\") just restarted.");
    } else {
      *out_session = it->second;
    }
  }
  return Status::OK();
}

Status SessionMgr::WorkerSessionForSession(
    const string& session_handle, std::shared_ptr<WorkerSession>* out_session) {
  mutex_lock l(mu_);
  return WorkerSessionForSessionLocked(session_handle, out_session);
}

std::shared_ptr<WorkerSession> SessionMgr::LegacySession() {
  return legacy_session_;
}

void SessionMgr::SetLogging(bool active) {
  mutex_lock l(mu_);
  this->is_logging_active_ = active;
  // Legacy Session
  if (legacy_session_) {
    auto* worker_cache = legacy_session_->worker_cache();
    if (worker_cache) {
      worker_cache->SetLogging(active);
    }
  }

  for (const auto& session_kv : sessions_) {
    auto session = session_kv.second.get();
    if (session) {
      auto* worker_cache = session->worker_cache();
      if (worker_cache) {
        worker_cache->SetLogging(active);
      }
    }
  }
}

void SessionMgr::RetrieveLogs(tensorflow::int64 step_id,
                              LoggingResponse* response) {
  mutex_lock l(mu_);
  // Legacy Session
  if (legacy_session_) {
    auto* worker_cache = legacy_session_->worker_cache();
    if (worker_cache) {
      auto step_stats = StepStats();
      if (worker_cache->RetrieveLogs(step_id, &step_stats)) {
        auto* labeled_step_stats = response->add_step();
        labeled_step_stats->set_step_id(step_id);
        labeled_step_stats->mutable_step_stats()->Swap(&step_stats);
      }
    }
  }
  for (const auto& session_kv : sessions_) {
    auto session = session_kv.second.get();
    if (session) {
      auto* worker_cache = session->worker_cache();
      if (worker_cache) {
        auto step_stats = StepStats();
        if (worker_cache->RetrieveLogs(step_id, &step_stats)) {
          auto* labeled_step_stats = response->add_step();
          labeled_step_stats->set_step_id(step_id);
          labeled_step_stats->mutable_step_stats()->Swap(&step_stats);
        }
      }
    }
  }
}

void SessionMgr::ClearLogs() {
  mutex_lock l(mu_);
  // Legacy Session
  if (legacy_session_) {
    auto* worker_cache = legacy_session_->worker_cache();
    if (worker_cache) {
      worker_cache->ClearLogs();
    }
  }

  for (const auto& session_kv : sessions_) {
    auto session = session_kv.second.get();
    if (session) {
      auto* worker_cache = session->worker_cache();
      if (worker_cache) {
        worker_cache->ClearLogs();
      }
    }
  }
}
}  // namespace tensorflow
