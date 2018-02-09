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
#include <iostream>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "tensorflow/contrib/lite/toco/graph_transformations/graph_transformations.h"
#include "tensorflow/contrib/lite/toco/graph_transformations/lstm_utils.h"
#include "tensorflow/contrib/lite/toco/model.h"
#include "tensorflow/contrib/lite/toco/tooling_util.h"

namespace toco {

bool SplitLstmCellInputs::Run(Model* model, std::size_t op_index) {
  // Find lstm cell.
  auto op_it = model->operators.begin() + op_index;
  auto curr_op = op_it->get();
  if (curr_op->type != OperatorType::kLstmCell) {
    return false;
  }

  // Already an extended LstmCell with kExtendedLstmInputCount of inputs,
  // do not need to split cell inputs.
  if (curr_op->inputs.size() == kExtendedLstmInputCount) {
    return false;
  }

  // Make sure the WEIGHTS_INPUT and BIASES_INPUT are constant arrays,
  // that are able to be split into smaller weight and bias tensors.
  if (!IsConstantParameterArray(
          *model, curr_op->inputs[LstmCellOperator::WEIGHTS_INPUT]) ||
      !IsConstantParameterArray(
          *model, curr_op->inputs[LstmCellOperator::BIASES_INPUT])) {
    return false;
  }

  // Make sure propagate_fixed_sizes has defined the size of the output.
  if (!model->GetArray(curr_op->outputs[LstmCellOperator::ACTIV_OUTPUT])
           .has_shape()) {
    return false;
  }

  // Emplace a new LstmCell operator with extended inputs (kernel/lstm.cc).
  auto lstm_cell_op = absl::make_unique<LstmCellOperator>();
  lstm_cell_op->inputs.resize(kExtendedLstmInputCount);
  int num_input = model->GetArray(curr_op->inputs[LstmCellOperator::DATA_INPUT])
                      .shape()
                      .dims(1);

  // n_cell and n_output have the same size when there is no projection.
  int num_cell =
      model->GetArray(curr_op->outputs[LstmCellOperator::ACTIV_OUTPUT])
          .shape()
          .dims(1);
  int num_output = num_cell;

  // Data input.
  lstm_cell_op->inputs[kInputTensor] =
      curr_op->inputs[LstmCellOperator::ACTIV_OUTPUT];

  // Get original weight tensor and decompose 1 tensor to 8 sub tensors.
  Array& kernel =
      model->GetArray(curr_op->inputs[LstmCellOperator::WEIGHTS_INPUT]);
  const string base_name(FindLongestCommonPrefix(
      curr_op->outputs[LstmCellOperator::ACTIV_OUTPUT],
      curr_op->outputs[LstmCellOperator::STATE_OUTPUT]));

  // Input weight tensors of size {n_cell, n_input}.
  CopySubArrayToArray(
      model, &(lstm_cell_op->inputs[kInputToInputWeightsTensor]),
      base_name + "weight_i_i", num_cell, num_input, kernel, 0, 0);
  CopySubArrayToArray(model, &(lstm_cell_op->inputs[kInputToCellWeightsTensor]),
                      base_name + "weight_c_i", num_cell, num_input, kernel,
                      num_cell, 0);
  CopySubArrayToArray(
      model, &(lstm_cell_op->inputs[kInputToForgetWeightsTensor]),
      base_name + "weight_f_i", num_cell, num_input, kernel, num_cell * 2, 0);
  CopySubArrayToArray(
      model, &(lstm_cell_op->inputs[kInputToOutputWeightsTensor]),
      base_name + "weight_o_i", num_cell, num_input, kernel, num_cell * 3, 0);

  // Recurrent weight tensors of size {n_cell, n_output}.
  CopySubArrayToArray(
      model, &(lstm_cell_op->inputs[kRecurrentToInputWeightsTensor]),
      base_name + "weight_i_r", num_cell, num_output, kernel, 0, num_input);
  CopySubArrayToArray(model,
                      &(lstm_cell_op->inputs[kRecurrentToCellWeightsTensor]),
                      base_name + "weight_c_r", num_cell, num_output, kernel,
                      num_cell, num_input);
  CopySubArrayToArray(model,
                      &(lstm_cell_op->inputs[kRecurrentToForgetWeightsTensor]),
                      base_name + "weight_f_r", num_cell, num_output, kernel,
                      num_cell * 2, num_input);
  CopySubArrayToArray(model,
                      &(lstm_cell_op->inputs[kRecurrentToOutputWeightsTensor]),
                      base_name + "weight_o_r", num_cell, num_output, kernel,
                      num_cell * 3, num_input);

  // Peephole (optional).
  CreateOptionalArray(model, &(lstm_cell_op->inputs[kCellToInputWeightsTensor]),
                      base_name + "peephole_c_i");
  CreateOptionalArray(model,
                      &(lstm_cell_op->inputs[kCellToForgetWeightsTensor]),
                      base_name + "peephole_c_f");
  CreateOptionalArray(model,
                      &(lstm_cell_op->inputs[kCellToOutputWeightsTensor]),
                      base_name + "peephole_c_o");

  // Get original bias tensor and decompose 1 tensor to 4 sub tensors
  Array& bias =
      model->GetArray(curr_op->inputs[LstmCellOperator::BIASES_INPUT]);
  CopySubArrayToArray(model, &(lstm_cell_op->inputs[kInputGateBiasTensor]),
                      base_name + "bias_i", num_cell, 1, bias, 0, 0);
  CopySubArrayToArray(model, &(lstm_cell_op->inputs[kCellGateBiasTensor]),
                      base_name + "bias_c", num_cell, 1, bias, num_cell, 0);
  CopySubArrayToArray(model, &(lstm_cell_op->inputs[kForgetGateBiasTensor]),
                      base_name + "bias_f", num_cell, 1, bias, num_cell * 2, 0);
  CopySubArrayToArray(model, &(lstm_cell_op->inputs[kOutputGateBiasTensor]),
                      base_name + "bias_o", num_cell, 1, bias, num_cell * 3, 0);

  // Projection (optional).
  CreateOptionalArray(model, &(lstm_cell_op->inputs[kProjectionWeightsTensor]),
                      base_name + "proj_weight");
  CreateOptionalArray(model, &(lstm_cell_op->inputs[kProjectionBiasTensor]),
                      base_name + "proj_bias");

  // Reorder LstmCell's outputs.
  lstm_cell_op->outputs.resize(LstmCellOperator::NUM_OUTPUTS);
  lstm_cell_op->outputs[kScratchBufferTensor] =
      curr_op->outputs[LstmCellOperator::CONCAT_TEMP];
  lstm_cell_op->outputs[kOutputStateTensor] =
      curr_op->outputs[LstmCellOperator::ACTIV_TEMP];
  lstm_cell_op->outputs[kCellStateTensor] =
      curr_op->outputs[LstmCellOperator::STATE_OUTPUT];
  lstm_cell_op->outputs[kOutputTensor] =
      curr_op->outputs[LstmCellOperator::ACTIV_OUTPUT];

  // Add the op into model.
  model->operators.emplace(op_it, std::move(lstm_cell_op));
  AddMessageF("Creating extended LstmCell replacing previous lstm cell");

  // Delete arrays and operators replaced by the LSTM cell operator. Order is
  // important - DeleteArrayIfUnused() only succeeds if dependent operators
  // have been removed first. Start at the output and work towards the input.
  // Erase curr lstm op being replaced.
  DeleteArrayIfUnused(curr_op->inputs[LstmCellOperator::WEIGHTS_INPUT], model);
  DeleteArrayIfUnused(curr_op->inputs[LstmCellOperator::BIASES_INPUT], model);
  DeleteArrayIfUnused(curr_op->inputs[LstmCellOperator::PREV_ACTIV_INPUT],
                      model);
  DeleteArrayIfUnused(curr_op->inputs[LstmCellOperator::PREV_STATE_INPUT],
                      model);
  model->operators.erase(FindOp(*model, curr_op));

  return true;
}

}  // namespace toco
