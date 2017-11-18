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

#include "tensorflow/core/grappler/op_types.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/lib/core/status.h"

namespace tensorflow {
namespace grappler {

bool IsAdd(const NodeDef& node) {
  const auto op = node.op();
  return op == "Add";
}

bool IsAddN(const NodeDef& node) {
  const auto op = node.op();
  return op == "AddN";
}

bool IsAvgPoolGrad(const NodeDef& node) {
  const auto op = node.op();
  return op == "AvgPoolGrad";
}

bool IsBiasAddGrad(const NodeDef& node) {
  const auto op = node.op();
  return op == "BiasAddGrad";
}

bool IsConcatOffset(const NodeDef& node) {
  const auto op = node.op();
  return op == "ConcatOffset";
}

bool IsConstant(const NodeDef& node) {
  const auto op = node.op();
  return op == "Const";
}

bool IsConv2D(const NodeDef& node) {
  const auto op = node.op();
  return op == "Conv2D";
}

bool IsConv2DBackpropFilter(const NodeDef& node) {
  const auto op = node.op();
  return op == "Conv2DBackpropFilter";
}

bool IsConv2DBackpropInput(const NodeDef& node) {
  const auto op = node.op();
  return op == "Conv2DBackpropInput";
}

bool IsDequeueOp(const NodeDef& node) {
  const auto& op = node.op();
  return op == "QueueDequeueManyV2" || op == "QueueDequeueMany" ||
         op == "QueueDequeueV2" || op == "QueueDequeue" ||
         op == "QueueDequeueUpToV2" || op == "QueueDequeueUpTo";
}

bool IsEnter(const NodeDef& node) {
  const auto& op = node.op();
  return op == "Enter" || op == "RefEnter";
}

bool IsExit(const NodeDef& node) {
  const auto& op = node.op();
  return op == "Exit" || op == "RefExit";
}

bool IsFloorMod(const NodeDef& node) {
  const auto& op = node.op();
  return op == "FloorMod";
}

bool IsFusedBatchNormGradV1(const NodeDef& node) {
  const auto& op = node.op();
  return op == "FusedBatchNormGrad";
}

bool IsIdentity(const NodeDef& node) {
  const auto& op = node.op();
  return op == "Identity" || op == "RefIdentity";
}

bool IsMerge(const NodeDef& node) {
  const auto op = node.op();
  return op == "Merge" || op == "RefMerge";
}

bool IsMul(const NodeDef& node) {
  const auto op = node.op();
  return op == "Mul";
}

bool IsNoOp(const NodeDef& node) {
  const auto op = node.op();
  return op == "NoOp";
}

bool IsNextIteration(const NodeDef& node) {
  const auto& op = node.op();
  return op == "NextIteration" || op == "RefNextIteration";
}

bool IsPad(const NodeDef& node) {
  const auto op = node.op();
  return op == "Pad";
}

bool IsPlaceholder(const NodeDef& node) {
  const auto op = node.op();
  return op == "Placeholder" || op == "PlaceholderV2" ||
         op == "PlaceholderWithDefault";
}

bool IsRealDiv(const NodeDef& node) {
  const auto op = node.op();
  return op == "RealDiv";
}

bool IsReluGrad(const NodeDef& node) {
  const auto op = node.op();
  return op == "ReluGrad";
}

bool IsRecv(const NodeDef& node) {
  const auto op = node.op();
  return op == "_Recv";
}

bool IsReduction(const NodeDef& node) {
  const auto& op = node.op();
  return op == "Sum" || op == "Prod" || op == "Min" || op == "Max" ||
         op == "Mean" || op == "Any" || op == "All";
}

bool IsReshape(const NodeDef& node) { return (node.op() == "Reshape"); }

bool IsRestore(const NodeDef& node) {
  return (node.op() == "Restore" || node.op() == "RestoreV2" ||
          node.op() == "RestoreSlice");
}

bool IsSend(const NodeDef& node) {
  const auto op = node.op();
  return op == "_Send";
}

bool IsSlice(const NodeDef& node) {
  const auto op = node.op();
  return op == "Slice";
}

bool IsSquaredDifference(const NodeDef& node) {
  const auto op = node.op();
  return op == "SquaredDifference";
}

bool IsSqueeze(const NodeDef& node) {
  const auto op = node.op();
  return op == "Squeeze";
}

bool IsStopGradient(const NodeDef& node) {
  const auto& op = node.op();
  return op == "StopGradient" || op == "PreventGradient";
}

bool IsSub(const NodeDef& node) {
  const auto op = node.op();
  return op == "Sub";
}

bool IsSum(const NodeDef& node) {
  const auto op = node.op();
  return op == "Sum";
}

bool IsSwitch(const NodeDef& node) {
  const auto& op = node.op();
  return op == "Switch" || op == "RefSwitch";
}

bool IsTranspose(const NodeDef& node) {
  const auto op = node.op();
  return op == "Transpose";
}

bool IsVariable(const NodeDef& node) {
  const auto op = node.op();
  return op == "Variable" || op == "VariableV2" || op == "AutoReloadVariable" ||
         op == "VarHandleOp" || op == "ReadVariableOp";
}

bool IsFreeOfSideEffect(const NodeDef& node) {
  // Placeholders must be preserved to keep the graph feedable.
  if (IsPlaceholder(node)) {
    return false;
  }
  const OpDef* op_def = nullptr;
  Status status = OpRegistry::Global()->LookUpOpDef(node.op(), &op_def);
  if (!status.ok()) {
    return false;
  }
  if (op_def->is_stateful()) {
    return false;
  }
  // Nodes such as Assign or AssignAdd modify one of their inputs.
  for (const auto& input : op_def->input_arg()) {
    if (input.is_ref()) {
      return false;
    }
  }
  return true;
}

bool ModifiesFrameInfo(const NodeDef& node) {
  return IsEnter(node) || IsExit(node) || IsNextIteration(node);
}

}  // end namespace grappler
}  // end namespace tensorflow
