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

#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/reader_op_kernel.h"
#include "tensorflow/core/util/proto/descriptor_pool_registry.h"

#include "tensorflow/core/util/proto/descriptors.h"

namespace tensorflow {
namespace {

// Build a `DescriptorPool` from the named file or URI. The file or URI
// must be available to the current TensorFlow environment.
//
// The file must contain a serialized `FileDescriptorSet`. See
// `GetDescriptorPool()` for more information.
Status GetDescriptorPoolFromFile(
    tensorflow::Env* env, const string& filename,
    std::unique_ptr<tensorflow::protobuf::DescriptorPool>* owned_desc_pool) {
  Status st = env->FileExists(filename);
  if (!st.ok()) {
    return st;
  }

  // Read and parse the FileDescriptorSet.
  tensorflow::protobuf::FileDescriptorSet descs;
  std::unique_ptr<tensorflow::ReadOnlyMemoryRegion> buf;
  st = env->NewReadOnlyMemoryRegionFromFile(filename, &buf);
  if (!st.ok()) {
    return st;
  }
  if (!descs.ParseFromArray(buf->data(), buf->length())) {
    return errors::InvalidArgument(
        "descriptor_source contains invalid FileDescriptorSet: ", filename);
  }

  // Build a DescriptorPool from the FileDescriptorSet.
  owned_desc_pool->reset(new tensorflow::protobuf::DescriptorPool());
  for (const auto& filedesc : descs.file()) {
    if ((*owned_desc_pool)->BuildFile(filedesc) == nullptr) {
      return errors::InvalidArgument(
          "Problem loading FileDescriptorProto (missing dependencies?): ",
          filename);
    }
  }
  return Status::OK();
}

}  // namespace

Status GetDescriptorPool(
    tensorflow::Env* env, string const& descriptor_source,
    tensorflow::protobuf::DescriptorPool const** desc_pool,
    std::unique_ptr<tensorflow::protobuf::DescriptorPool>* owned_desc_pool) {
  // Attempt to lookup the pool in the registry.
  auto pool_fn = DescriptorPoolRegistry::Global()->Get(descriptor_source);
  if (pool_fn != nullptr) {
    return (*pool_fn)(desc_pool, owned_desc_pool);
  }

  // If there is no pool function registered for the given source, let the
  // runtime find the file or URL.
  Status status =
      GetDescriptorPoolFromFile(env, descriptor_source, owned_desc_pool);
  if (status.ok()) {
    *desc_pool = owned_desc_pool->get();
  }
  *desc_pool = owned_desc_pool->get();
  return status;
}

}  // namespace tensorflow
