/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/data/service/test_util.h"

#include "tensorflow/core/data/dataset_test_base.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/path.h"

namespace tensorflow {
namespace data {
namespace test_util {

namespace {
constexpr char kTestdataDir[] =
    "tensorflow/core/data/service/testdata";

// Proto content generated by
//
// import tensorflow.compat.v2 as tf
// tf.enable_v2_behavior()
//
// ds = tf.data.Dataset.range(10)
// ds = ds.map(lambda x: x*x)
// g = tf.compat.v1.GraphDef()
// g.ParseFromString(ds._as_serialized_graph().numpy())
// print(g)
constexpr char kMapGraphDefFile[] = "map_graph_def.pbtxt";
}  // namespace

Status map_test_case(GraphDefTestCase* test_case) {
  std::string filepath = io::JoinPath(kTestdataDir, kMapGraphDefFile);
  GraphDef graph_def;
  TF_RETURN_IF_ERROR(ReadTextProto(Env::Default(), filepath, &graph_def));
  int num_elements = 10;
  std::vector<std::vector<Tensor>> outputs(num_elements);
  for (int i = 0; i < num_elements; ++i) {
    outputs[i] = CreateTensors<int64>(TensorShape{}, {{i * i}});
  }
  *test_case = {"MapGraph", graph_def, outputs};
  return Status::OK();
}

}  // namespace test_util
}  // namespace data
}  // namespace tensorflow
