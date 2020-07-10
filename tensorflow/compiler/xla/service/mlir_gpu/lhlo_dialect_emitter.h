/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_MLIR_GPU_LHLO_DIALECT_EMITTER_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_MLIR_GPU_LHLO_DIALECT_EMITTER_H_

#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/Function.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/IR/Module.h"  // from @llvm-project
#include "tensorflow/compiler/xla/service/buffer_assignment.h"
#include "tensorflow/compiler/xla/service/gpu/thunk.h"
#include "tensorflow/compiler/xla/service/gpu/thunk_emitter.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/mlir_gpu/emission_context.h"
#include "tensorflow/compiler/xla/status.h"

namespace xla {
namespace mlir_gpu {

// Implementation for the translation of HLO instructions to a ThunkSequence
// via MLIR using the LHLO dialect.
// Implements the DfsHloVisitor interface, emits LHLO computations as MLIR IR
// functions and transforms them into gpu::Thunk.
class LhloDialectEmitter : public DfsHloVisitorWithDefault,
                           private gpu::ThunkEmitter::EmissionContext {
 public:
  LhloDialectEmitter(xla::mlir_gpu::EmissionContext* emission_context,
                     const BufferAssignment& assignment,
                     const se::Platform* platform,
                     ::mlir::ModuleOp mlir_module);
  ~LhloDialectEmitter() override = default;

  // The following methods implement the DfsHloVisitor interface.
  //
  // Default action which emits code for most operations. Operations which are
  // special in some way are handled explicitly in HandleFoo methods.
  Status DefaultAction(HloInstruction* instr) override;
  Status HandleBroadcast(HloInstruction* instr) override;
  Status HandleCompare(HloInstruction* instr) override;
  Status HandleConcatenate(HloInstruction* instr) override;
  Status HandleConstant(HloInstruction* instr) override;
  Status HandleCustomCall(HloInstruction* instr) override;
  Status HandleFusion(HloInstruction* instr) override;
  Status HandleGather(HloInstruction* instr) override;
  Status HandleIota(HloInstruction* instr) override;
  Status HandleParameter(HloInstruction* instr) override;
  Status HandleReduce(HloInstruction* instr) override;
  Status HandleReduceWindow(HloInstruction* instr) override;
  Status HandleSelectAndScatter(HloInstruction* instr) override;
  Status HandleTuple(HloInstruction* instr) override;

  Status FinishVisit(HloInstruction* root) override;

  // Transfers the ownship of thunk_sequence_ out.
  gpu::ThunkSequence ConsumeThunkSequence() {
    gpu::ThunkSequence result;
    std::swap(result, thunk_sequence_);
    return result;
  }

  const absl::flat_hash_map<const xla::HloInstruction*, ::mlir::FuncOp>&
  InstructionToFunctionMap() const {
    return instruction_to_mlir_func_;
  }

 private:
  StatusOr<::mlir::FuncOp> CreateFunction(const HloInstruction& instr);
  // Interface required by ThunkEmitter
  void AddThunkToThunkSequence(std::unique_ptr<gpu::Thunk> thunk) override;
  StatusOr<BufferAllocation::Slice> MaybeGetAllocationSlice(
      const HloInstruction& hlo, const ShapeIndex& index) const override;
  int64 ByteSizeOf(const Shape& shape) const override;
  absl::string_view platform_name() const override;

  mlir::Location getLocation(const HloInstruction* instr) const;

  xla::mlir_gpu::EmissionContext* emission_context_;
  ::mlir::ModuleOp mlir_module_;
  ::mlir::Builder builder_;
  absl::flat_hash_map<const xla::HloInstruction*, ::mlir::FuncOp>
      instruction_to_mlir_func_;
  const BufferAssignment& buffer_assignment_;
  const se::Platform* platform_;
  // Cached pointer size extracted from the mlir module.
  unsigned pointer_size_;
  // The thunk sequence this IrEmitter generates for the input computation.
  gpu::ThunkSequence thunk_sequence_;

  TF_DISALLOW_COPY_AND_ASSIGN(LhloDialectEmitter);
};

}  // namespace mlir_gpu
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_MLIR_GPU_LHLO_DIALECT_EMITTER_H_
