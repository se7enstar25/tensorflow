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

// XLA-specific base classes for Reduction Ops.

#ifndef TENSORFLOW_COMPILER_TF2XLA_KERNELS_REDUCTION_OPS_H_
#define TENSORFLOW_COMPILER_TF2XLA_KERNELS_REDUCTION_OPS_H_

#include "tensorflow/compiler/tf2xla/xla_op_kernel.h"
#include "tensorflow/compiler/xla/client/computation_builder.h"
#include "tensorflow/core/framework/op_kernel.h"

namespace tensorflow {

// Reduction operations. The base class contains pure virtual methods
// to override: description is a textual description of the mapped
// function; InitialValue constructs the base case for the reduction;
// BuildReducer adds the implementation of the reduction lambda to a
// xla::ComputationBuilder and BuildFinalizer adds the
// implementation of the finalizer lambda (if there is one) to a
// xla::ComputationBuilder.
class XlaReductionOp : public XlaOpKernel {
 public:
  explicit XlaReductionOp(OpKernelConstruction* ctx);
  ~XlaReductionOp() override {}

  // Return the base case for the reduction. Defaults to zero.
  virtual xla::ComputationDataHandle InitialValue(
      xla::ComputationBuilder* builder);

  // Implement the (scalar,scalar)->scalar lambda that should be
  // applied to each pair of elements to be reduced. The desired
  // computation should be added to 'builder' and
  // '(scalar_lhs,scalar_rhs)' are the function's inputs.
  virtual void BuildReducer(xla::ComputationBuilder* builder,
                            const xla::ComputationDataHandle& scalar_lhs,
                            const xla::ComputationDataHandle& scalar_rhs) = 0;

  // Implement the scalar->scalar lambda that should be applied to
  // each element to be finalized. The desired computation should be
  // added to 'builder' and 'scalar_argument' is the function's
  // input. 'num_elements_reduced' is the number of elements that contributed
  // to the reduction. If the reduction has a finalizer return true, otherwise
  // return false and any computation added to builder will be
  // ignored. Defaults to return false.
  virtual bool BuildFinalizer(xla::ComputationBuilder* builder,
                              const xla::ComputationDataHandle& scalar_argument,
                              int64 num_elements_reduced);

  void Compile(XlaOpKernelContext* ctx) override;

 private:
  // True if the number of dimensions should be maintained.
  bool keep_dims_;
};

}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_TF2XLA_KERNELS_REDUCTION_OPS_H_
