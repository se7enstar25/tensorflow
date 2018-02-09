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

bool MergeLstmCellInputs::Run(Model* model, std::size_t op_index) {
  // Find lstm cell.
  auto op_it = model->operators.begin() + op_index;
  auto src_op = op_it->get();
  if (src_op->type != OperatorType::kLstmCell) {
    return false;
  }

  // Already a compact LstmCell with LstmCellOperator::NUM_INPUTS of inputs,
  // do not need to merge cell inputs.
  if (src_op->inputs.size() == LstmCellOperator::NUM_INPUTS) {
    return false;
  }

  // Identify prev_activ_input, prev_state_input as required Op inputs,
  // using the rnn_states in the model flag.
  string prev_activ_input;
  if (!GetMatchingRnnArray(model, src_op->outputs[kOutputTensor],
                           &prev_activ_input)) {
    return false;
  }
  string prev_state_input;
  if (!GetMatchingRnnArray(model, src_op->outputs[kCellStateTensor],
                           &prev_state_input)) {
    return false;
  }

  // Get LstmCell's cell, input, output size.
  int num_cell = model->GetArray(src_op->inputs[kInputToInputWeightsTensor])
                     .shape()
                     .dims(0);
  int num_input = model->GetArray(src_op->inputs[kInputToInputWeightsTensor])
                      .shape()
                      .dims(1);
  int num_output =
      model->GetArray(src_op->inputs[kRecurrentToInputWeightsTensor])
          .shape()
          .dims(1);

  // Make sure n_cell and n_output are equal as there is no projection.
  CHECK_EQ(num_cell, num_output);

  // Create tensorflow_graphdef style's one big weight tensor.
  const string base_name(FindLongestCommonPrefix(
      src_op->outputs[kOutputTensor], src_op->outputs[kCellStateTensor]));
  string merged_weights = AvailableArrayName(*model, base_name + "weights");
  auto& array = model->GetOrCreateArray(merged_weights);
  array.data_type = ArrayDataType::kFloat;
  int weights_dim1 = 4 * num_cell;
  int weights_dim2 = num_input + num_output;
  Shape shape = Shape({weights_dim1, weights_dim2});
  array.copy_shape(shape);
  auto& buffer = array.GetMutableBuffer<ArrayDataType::kFloat>();
  buffer.data.resize(weights_dim1 * weights_dim2);

  // Merge 8 small weight tensors to 1 weight tensor.
  CopyArrayToSubArray(
      buffer, weights_dim2,
      model->GetArray(src_op->inputs[kInputToInputWeightsTensor]), 0, 0);
  CopyArrayToSubArray(
      buffer, weights_dim2,
      model->GetArray(src_op->inputs[kInputToCellWeightsTensor]), num_cell, 0);
  CopyArrayToSubArray(
      buffer, weights_dim2,
      model->GetArray(src_op->inputs[kInputToForgetWeightsTensor]),
      num_cell * 2, 0);
  CopyArrayToSubArray(
      buffer, weights_dim2,
      model->GetArray(src_op->inputs[kInputToOutputWeightsTensor]),
      num_cell * 3, 0);
  CopyArrayToSubArray(
      buffer, weights_dim2,
      model->GetArray(src_op->inputs[kRecurrentToInputWeightsTensor]), 0,
      num_input);
  CopyArrayToSubArray(
      buffer, weights_dim2,
      model->GetArray(src_op->inputs[kRecurrentToCellWeightsTensor]), num_cell,
      num_input);
  CopyArrayToSubArray(
      buffer, weights_dim2,
      model->GetArray(src_op->inputs[kRecurrentToForgetWeightsTensor]),
      num_cell * 2, num_input);
  CopyArrayToSubArray(
      buffer, weights_dim2,
      model->GetArray(src_op->inputs[kRecurrentToOutputWeightsTensor]),
      num_cell * 3, num_input);

  // Create tensorflow_graphdef style's one big bias tensor.
  string merged_biases = AvailableArrayName(*model, base_name + "biases");
  auto& bias_array = model->GetOrCreateArray(merged_biases);
  bias_array.data_type = ArrayDataType::kFloat;
  bias_array.copy_shape(Shape({weights_dim1}));
  auto& bias_buffer = bias_array.GetMutableBuffer<ArrayDataType::kFloat>();
  bias_buffer.data.resize(weights_dim1);

  // Merge 4 small bias tensors into a big one.
  CopyArrayToSubArray(bias_buffer, weights_dim2,
                      model->GetArray(src_op->inputs[kInputGateBiasTensor]), 0,
                      0);
  CopyArrayToSubArray(bias_buffer, weights_dim2,
                      model->GetArray(src_op->inputs[kCellGateBiasTensor]),
                      num_cell, 0);
  CopyArrayToSubArray(bias_buffer, weights_dim2,
                      model->GetArray(src_op->inputs[kForgetGateBiasTensor]),
                      num_cell * 2, 0);
  CopyArrayToSubArray(bias_buffer, weights_dim2,
                      model->GetArray(src_op->inputs[kOutputGateBiasTensor]),
                      num_cell * 3, 0);

  // Emplace a new LSTM cell operator (use basic 5 inputs kernel).
  auto lstm_cell_op = absl::make_unique<LstmCellOperator>();

  // Compact LstmCell's 5 inputs.
  lstm_cell_op->inputs.resize(LstmCellOperator::NUM_INPUTS);
  lstm_cell_op->inputs[LstmCellOperator::DATA_INPUT] =
      src_op->inputs[kInputTensor];
  lstm_cell_op->inputs[LstmCellOperator::WEIGHTS_INPUT] = merged_weights;
  lstm_cell_op->inputs[LstmCellOperator::BIASES_INPUT] = merged_biases;
  lstm_cell_op->inputs[LstmCellOperator::PREV_ACTIV_INPUT] = prev_activ_input;
  lstm_cell_op->inputs[LstmCellOperator::PREV_STATE_INPUT] = prev_state_input;

  // Reorder LstmCell's 4 outputs.
  lstm_cell_op->outputs.resize(LstmCellOperator::NUM_OUTPUTS);
  lstm_cell_op->outputs[LstmCellOperator::ACTIV_OUTPUT] =
      src_op->outputs[kOutputTensor];
  lstm_cell_op->outputs[LstmCellOperator::STATE_OUTPUT] =
      src_op->outputs[kCellStateTensor];
  lstm_cell_op->outputs[LstmCellOperator::CONCAT_TEMP] =
      src_op->outputs[kScratchBufferTensor];
  lstm_cell_op->outputs[LstmCellOperator::ACTIV_TEMP] =
      src_op->outputs[kOutputStateTensor];

  // Add the op into model.
  model->operators.emplace(op_it, std::move(lstm_cell_op));
  AddMessageF("Creating compact LstmCell replacing previous lstm cell");

  // Delete arrays and operators replaced by the LSTM cell operator. Order is
  // important - DeleteArrayIfUnused() only succeeds if dependent operators
  // have been removed first. Start at the output and work towards the input.
  // Erase curr lstm op being replaced.
  DeleteArrayIfUnused(src_op->inputs[kInputToInputWeightsTensor], model);
  DeleteArrayIfUnused(src_op->inputs[kInputToForgetWeightsTensor], model);
  DeleteArrayIfUnused(src_op->inputs[kInputToCellWeightsTensor], model);
  DeleteArrayIfUnused(src_op->inputs[kInputToOutputWeightsTensor], model);
  DeleteArrayIfUnused(src_op->inputs[kRecurrentToInputWeightsTensor], model);
  DeleteArrayIfUnused(src_op->inputs[kRecurrentToForgetWeightsTensor], model);
  DeleteArrayIfUnused(src_op->inputs[kRecurrentToCellWeightsTensor], model);
  DeleteArrayIfUnused(src_op->inputs[kRecurrentToOutputWeightsTensor], model);
  DeleteArrayIfUnused(src_op->inputs[kInputGateBiasTensor], model);
  DeleteArrayIfUnused(src_op->inputs[kForgetGateBiasTensor], model);
  DeleteArrayIfUnused(src_op->inputs[kCellGateBiasTensor], model);
  DeleteArrayIfUnused(src_op->inputs[kOutputGateBiasTensor], model);
  model->operators.erase(FindOp(*model, src_op));

  return true;
}

}  // namespace toco
