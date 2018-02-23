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

#ifndef TENSORFLOW_CONTRIB_TENSORRT_RESOURCES_TRTRESOURCEMANAGER_H_

#define TENSORFLOW_CONTRIB_TENSORRT_RESOURCE_TRTRESOURCEMANAGER_H_
#include <memory>

#include <string>
#include <unordered_map>
#include "tensorflow/core/framework/resource_mgr.h"
#include "tensorflow/core/platform/mutex.h"

namespace tensorflow {
namespace trt {
class TRTResourceManager {
  TRTResourceManager() = default;

 public:
  static std::shared_ptr<TRTResourceManager> instance() {
    static std::shared_ptr<TRTResourceManager> instance_(
        new TRTResourceManager);
    return instance_;
  }
  // returns a manager for given op, if it doesn't exists it creates one
  std::shared_ptr<tensorflow::ResourceMgr> getManager(const string& op_name);

 private:
  std::unordered_map<string, std::shared_ptr<tensorflow::ResourceMgr>>
      managers_;
  tensorflow::mutex map_mutex_;
};
}  // namespace trt
}  // namespace tensorflow
#endif  // TENSORFLOW_CONTRIB_TENSORRT_RESOURCES_TRTRESOURCEMANAGER_H_
