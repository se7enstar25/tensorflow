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

#include "tensorflow/compiler/xla/service/hlo_get_dimension_size_rewriter.h"

#include "absl/algorithm/container.h"
#include "tensorflow/compiler/xla/literal_util.h"
#include "tensorflow/compiler/xla/service/dynamic_dimension_inference.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/hlo_opcode.h"
#include "tensorflow/compiler/xla/service/shape_inference.h"

namespace xla {

namespace {

StatusOr<bool> ReplaceGetSize(
    HloInstruction* instr,
    const DynamicDimensionInference* dynamic_dimension_inference) {
  if (instr->opcode() != HloOpcode::kGetDimensionSize) {
    return false;
  }
  HloComputation* computation = instr->parent();

  TF_ASSIGN_OR_RETURN(auto legal_shape,
                      ShapeInference::InferGetDimensionSizeShape(
                          instr->operand(0)->shape(), instr->dimension()));
  TF_RET_CHECK(ShapeUtil::Equal(instr->shape(), legal_shape))
      << "instr->shape() " << instr->shape().ToString() << " , "
      << "legal_shape " << legal_shape.ToString();
  TF_RET_CHECK(ShapeUtil::HasPrimitiveType(instr->shape(), S32));
  HloInstruction* operand = instr->mutable_operand(0);
  int64 dim = instr->dimension();
  HloInstruction* dynamic_size =
      dynamic_dimension_inference->GetDynamicSize(operand, {}, dim);
  if (dynamic_size != nullptr) {
    TF_RETURN_IF_ERROR(instr->ReplaceAllUsesWith(dynamic_size));
  } else {
    int32 size = instr->operand(0)->shape().dimensions(dim);
    HloInstruction* new_instr = computation->AddInstruction(
        HloInstruction::CreateConstant(LiteralUtil::CreateR0<int32>(size)));
    TF_RETURN_IF_ERROR(instr->ReplaceAllUsesWith(new_instr));
  }
  return true;
}

}  // namespace

StatusOr<bool> HloGetDimensionSizeRewriter::Run(HloModule* module) {
  bool changed = false;
  HloProto proto;
  TF_ASSIGN_OR_RETURN(DynamicDimensionInference inference,
                      DynamicDimensionInference::Run(module));
  *proto.mutable_hlo_module() = module->ToProto();
  for (auto* computation : module->computations()) {
    for (auto instruction : computation->instructions()) {
      TF_ASSIGN_OR_RETURN(bool replaced,
                          ReplaceGetSize(instruction, &inference));
      changed = changed || replaced;
    }
  }
  return changed;
}

}  // namespace xla
