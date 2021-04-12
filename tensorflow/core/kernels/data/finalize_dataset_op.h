/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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
#ifndef TENSORFLOW_CORE_KERNELS_DATA_FINALIZE_DATASET_OP_H_
#define TENSORFLOW_CORE_KERNELS_DATA_FINALIZE_DATASET_OP_H_

#include "tensorflow/core/framework/dataset.h"
#include "tensorflow/core/framework/op_kernel.h"

namespace tensorflow {
namespace data {

// TODO(jsimsa): Provide class-level documentation for this and the other ops.
class FinalizeDatasetOp : public DatasetOpKernel {
 public:
  static constexpr const char* const kDatasetType = "Finalize";
  static constexpr const char* const kInputDataset = "input_dataset";
  static constexpr const char* const kOutputTypes = "output_types";
  static constexpr const char* const kOutputShapes = "output_shapes";
  // TODO(wilsin): Implement has_captured_ref using GraphDef representation
  // rather than relying on python check.
  static constexpr const char* const kHasCapturedRef = "has_captured_ref";

  explicit FinalizeDatasetOp(OpKernelConstruction* ctx);

  void MakeDataset(OpKernelContext* ctx, DatasetBase** output) override;

 private:
  class Dataset;
  bool has_captured_ref_;
};

class FinalizeDatasetNoopOp : public UnaryDatasetOpKernel {
 public:
  explicit FinalizeDatasetNoopOp(OpKernelConstruction* ctx)
      : UnaryDatasetOpKernel(ctx) {}

 protected:
  void MakeDataset(OpKernelContext* ctx, DatasetBase* input,
                   DatasetBase** output) override {
    LOG(WARNING) << "FinalizeDataset is only supported on CPU. Using it on "
                    "devices other than CPU has no effect.";
    input->Ref();
    *output = input;
  }
};

}  // namespace data
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_KERNELS_DATA_FINALIZE_DATASET_OP_H_
