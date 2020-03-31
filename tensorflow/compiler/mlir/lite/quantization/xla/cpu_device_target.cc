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

#include "tensorflow/compiler/mlir/lite/quantization/xla/cpu_device_target.h"

#include "mlir/Dialect/Quant/QuantOps.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/Support/LogicalResult.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/lite/quantization/quantization_context.h"

namespace mlir {
namespace xla_hlo {

namespace ph = std::placeholders;

CpuDeviceTarget::CpuDeviceTarget(MLIRContext* ctx) : DeviceTarget(ctx) {
  RegisterKernel("generic.concat", {qi8_, qi8_, qi8_},
                 quant::ScaleConstraintType::OutputInputSameScale);
  RegisterKernel("generic.mul", {qi8_, qi8_, qi8_},
                 quant::ScaleConstraintType::OutputInputFreeScale);
  RegisterKernel("generic.mul_add", {qi8_, qi8n_, any_, qi8_},
                 std::bind(&CpuDeviceTarget::HandleMultiplyAccumulateScale,
                           this, ph::_1, ph::_2, ph::_3, ph::_4));
  RegisterKernel("generic.matmul_add", {qi8_, qi8n_, any_, qi8_},
                 std::bind(&CpuDeviceTarget::HandleMultiplyAccumulateScale,
                           this, ph::_1, ph::_2, ph::_3, ph::_4));
}

LogicalResult CpuDeviceTarget::HandleMultiplyAccumulateScale(
    quant::QuantizeContext* ctx, Operation* op,
    quant::AdjacentOperations* new_items, bool* changed) {
  auto bias_params = ctx->GetOperandParams(op, 2);
  if (!EmptyParams(bias_params)) {
    return success();
  }
  std::vector<quant::QuantParams> op_types{ctx->GetOperandParams(op, 0),
                                           ctx->GetOperandParams(op, 1)};
  auto bias_scale = GetUniformQuantizedTypeForBias(op_types);
  if (bias_scale && ctx->SetOperandParams(op, 2, bias_scale)) {
    *changed = true;
    new_items->push_back(op->getOperand(2).getDefiningOp());
  }
  return success();
}

}  // namespace xla_hlo
}  // namespace mlir
