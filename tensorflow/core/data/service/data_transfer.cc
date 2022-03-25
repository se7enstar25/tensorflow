/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/data/service/data_transfer.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "absl/strings/str_join.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/mutex.h"
#include "tensorflow/core/platform/status.h"

namespace tensorflow {
namespace data {

namespace {
mutex* get_lock() {
  static mutex lock(LINKER_INITIALIZED);
  return &lock;
}

using DataTransferServerFactories =
    std::unordered_map<std::string,
                       std::function<std::shared_ptr<DataTransferServer>(
                           DataTransferServer::GetElementT)>>;
DataTransferServerFactories& transfer_server_factories() {
  static auto& factories = *new DataTransferServerFactories();
  return factories;
}

using DataTransferClientFactories =
    std::unordered_map<std::string, DataTransferClient::FactoryT>;
DataTransferClientFactories& transfer_client_factories() {
  static auto& factories = *new DataTransferClientFactories();
  return factories;
}
}  // namespace

GetElementResult GetElementResult::Copy() const {
  GetElementResult copy;
  copy.components = components;
  copy.element_index = element_index;
  copy.end_of_sequence = end_of_sequence;
  copy.skip = skip;
  return copy;
}

size_t GetElementResult::EstimatedMemoryUsageBytes() const {
  size_t size_bytes = components.size() * sizeof(Tensor) + sizeof(int64) +
                      sizeof(bool) + sizeof(bool);
  for (const Tensor& tensor : components) {
    size_bytes += tensor.AllocatedBytes();
  }
  return size_bytes;
}

void DataTransferServer::Register(
    std::string name,
    std::function<std::shared_ptr<DataTransferServer>(GetElementT)> factory) {
  mutex_lock l(*get_lock());
  if (!transfer_server_factories().insert({name, factory}).second) {
    LOG(ERROR)
        << "Two data transfer server factories are being registered with name "
        << name << ". Which one gets used is undefined.";
  }
}

Status DataTransferServer::Build(std::string name, GetElementT get_element,
                                 std::shared_ptr<DataTransferServer>* out) {
  mutex_lock l(*get_lock());
  auto it = transfer_server_factories().find(name);
  if (it != transfer_server_factories().end()) {
    *out = it->second(get_element);
    return Status::OK();
  }

  std::vector<string> available_names;
  for (const auto& factory : transfer_server_factories()) {
    available_names.push_back(factory.first);
  }

  return errors::NotFound(
      "No data transfer server factory has been registered for name ", name,
      ". The available names are: [ ", absl::StrJoin(available_names, ", "),
      " ]");
}

void DataTransferClient::Register(std::string name, FactoryT factory) {
  mutex_lock l(*get_lock());
  if (!transfer_client_factories().insert({name, factory}).second) {
    LOG(ERROR)
        << "Two data transfer client factories are being registered with name "
        << name << ". Which one gets used is undefined.";
  }
}

Status DataTransferClient::Build(std::string name, Config config,
                                 std::unique_ptr<DataTransferClient>* out) {
  mutex_lock l(*get_lock());
  auto it = transfer_client_factories().find(name);
  if (it != transfer_client_factories().end()) {
    return it->second(config, out);
  }

  std::vector<string> available_names;
  for (const auto& factory : transfer_client_factories()) {
    available_names.push_back(factory.first);
  }

  return errors::NotFound(
      "No data transfer client factory has been registered for name ", name,
      ". The available names are: [ ", absl::StrJoin(available_names, ", "),
      " ]");
}

}  // namespace data
}  // namespace tensorflow
