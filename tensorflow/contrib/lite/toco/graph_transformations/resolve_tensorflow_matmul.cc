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
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "tensorflow/contrib/lite/toco/graph_transformations/graph_transformations.h"
#include "tensorflow/contrib/lite/toco/model.h"
#include "tensorflow/contrib/lite/toco/tooling_util.h"
#include "tensorflow/core/platform/logging.h"

namespace toco {

bool ResolveTensorFlowMatMul::Run(Model* model, std::size_t op_index) {
  auto matmul_it = model->operators.begin() + op_index;
  if (matmul_it->get()->type != OperatorType::kTensorFlowMatMul) {
    return false;
  }
  const auto* matmul_op = matmul_it->get();

  // Find the op producing the array passed to this MatMul
  auto previous_op_it = model->operators.begin();
  bool found = false;
  for (; previous_op_it != model->operators.end(); ++previous_op_it) {
    for (const auto& output : (*previous_op_it)->outputs) {
      if (output == matmul_op->inputs[0]) {
        found = true;
        break;
      }
    }
    if (found) {
      break;
    }
  }
  Operator* previous_op = (found) ? previous_op_it->get() : nullptr;

  // construct the new FullyConnectedOperator
  auto* fc_op = new FullyConnectedOperator;
  fc_op->outputs = matmul_op->outputs;

  // insert the newly constructed FullyConnectedOperator
  auto fc_it = model->operators.emplace(matmul_it, fc_op);

  // refresh invalidated iterator
  matmul_it = fc_it + 1;
  DCHECK_EQ(matmul_it->get(), matmul_op);

  // The way that TensorFlow encodes FullyConnected ops is as a pair
  // (Reshape, MatMul), so we want to remove the Reshape op and rewrite the
  // MatMul
  // op as a FullyConnected. However, TensorFlow skips the Reshape ops if the
  // input doesn't need reshaping, so we can't just match (Reshape, MatMul)
  // pairs.
  if (previous_op && previous_op->type == OperatorType::kTensorFlowReshape) {
    AddMessageF("Combining %s and %s into %s", LogName(*previous_op),
                LogName(*matmul_op), LogName(*fc_op));
    const auto& previous_op_output = previous_op->outputs[0];
    if (CountOpsWithInput(*model, previous_op_output) == 1) {
      model->EraseArray(previous_op_output);
    }
    CHECK_EQ(previous_op->inputs.size(), 2);
    fc_op->inputs = {previous_op->inputs[0], matmul_op->inputs[1]};
    // Only remove Reshape node if no other node uses its output.
    if (CountOpsWithInput(*model, previous_op_output) == 1) {
      const auto& previous_op_shape = previous_op->inputs[1];
      if (CountOpsWithInput(*model, previous_op_shape) == 1 &&
          !GetOpWithOutput(*model, previous_op_shape)) {
        model->EraseArray(previous_op_shape);
      }
      model->operators.erase(previous_op_it);
    }

    // We may have just invalidated matmul_it, so let's refresh it now.
    matmul_it = model->operators.begin();
    for (; matmul_it != model->operators.end(); ++matmul_it) {
      if (matmul_it->get() == matmul_op) {
        break;
      }
    }
    CHECK(matmul_it != model->operators.end());
    CHECK(matmul_it->get() == matmul_op);
  } else {
    AddMessageF("Replacing %s by a FullyConnected operator",
                LogName(*matmul_op));
    fc_op->inputs = {matmul_op->inputs[0], matmul_op->inputs[1]};
  }

  // erase the MatMul operator
  model->operators.erase(matmul_it);
  return true;
}

}  // namespace toco
