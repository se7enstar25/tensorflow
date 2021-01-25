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

#ifndef TENSORFLOW_CORE_DATA_SERVICE_DATA_TRANSFER_H_
#define TENSORFLOW_CORE_DATA_SERVICE_DATA_TRANSFER_H_

#include <functional>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "tensorflow/core/data/dataset.pb.h"
#include "tensorflow/core/data/service/worker.pb.h"
#include "tensorflow/core/platform/status.h"

namespace tensorflow {
namespace data {

// Client for communicating with the tf.data service transfer server.
class DataTransferClient {
 public:
  struct Config {
    absl::string_view protocol;
    std::string address;
  };
  using FactoryT =
      std::function<Status(Config, std::unique_ptr<DataTransferClient>*)>;
  virtual ~DataTransferClient() = default;

  // Fetches the next element for the specified task_id. The element's
  // compressed tensors will be stored in `element`. If no element is available,
  // `end_of_sequence` will be `true`, and `element` will be left unchanged.
  virtual Status GetElement(int64 task_id, absl::optional<int64> consumer_index,
                            absl::optional<int64> round_index,
                            tensorflow::data::CompressedElement& element,
                            bool& end_of_sequence) = 0;

  // Makes a best effort to cancel all outstanding calls in progress for the
  // client, and causes further calls to return Cancelled status.
  virtual void TryCancel() = 0;

  // Registers a DataTransferClient factory under `name`.
  static void Register(std::string name, FactoryT factory);

  // Builds a DataTransferClient from the factory registered under `name`.
  static Status Build(std::string name, Config config,
                      std::unique_ptr<DataTransferClient>* out);
};

// Server for communicating with the tf.data service transfer client.
class DataTransferServer {
 public:
  using GetElementT =
      std::function<Status(const GetElementRequest*, GetElementResponse*)>;
  virtual ~DataTransferServer() = default;

  // Starts DataTransferServer, it should be available for requests afterwards.
  virtual Status Start() = 0;

  // Return the port that this server is listening on.
  virtual int get_port() = 0;

  // Register a DataTransferServer factory under `name`.
  static void Register(
      std::string name,
      std::function<std::shared_ptr<DataTransferServer>(GetElementT)> factory);

  // Builds a DataTransferServer from the factory registered with `name`.
  static Status Build(std::string name, GetElementT get_element,
                      std::shared_ptr<DataTransferServer>* out);
};

}  // namespace data
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_DATA_SERVICE_DATA_TRANSFER_H_
