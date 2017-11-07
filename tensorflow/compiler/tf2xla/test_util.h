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

// Helper functions for tests.

#ifndef TENSORFLOW_COMPILER_TF2XLA_TEST_UTIL_H_
#define TENSORFLOW_COMPILER_TF2XLA_TEST_UTIL_H_

#include <map>
#include <unordered_map>
#include <vector>

#include "tensorflow/core/framework/function.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/lib/core/status.h"

namespace tensorflow {

// Same as InstantiationResult, but has a GraphDef instead of just nodes.
struct InstantiationResultForTest {
  DataTypeVector arg_types;
  DataTypeVector ret_types;
  GraphDef gdef;
};

// Instantiates a function, producing a GraphDef to compare against the
// expected graph.
Status InstantiateFunctionForTest(const string& name,
                                  const FunctionLibraryDefinition& library,
                                  InstantiationResultForTest* result);

}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_TF2XLA_TEST_UTIL_H_
