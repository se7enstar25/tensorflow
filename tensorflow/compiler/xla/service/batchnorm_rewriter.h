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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_BATCHNORM_REWRITER_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_BATCHNORM_REWRITER_H_

#include <utility>

#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_pass_interface.h"

namespace xla {

// A pass which rewrites batch norm operations into more operations. Breaking a
// big operation into smaller operations helps leverage our generic fusion
// logic.
class BatchNormRewriter : public HloPassInterface {
 public:
  // When use_fusion is set, a multi-output fusion node is created.
  BatchNormRewriter(bool rewrite_training_op = false,
                    bool rewrite_grad_op = false, bool use_fusion = true)
      : rewrite_training_op_(rewrite_training_op),
        rewrite_grad_op_(rewrite_grad_op),
        use_fusion_(use_fusion) {}
  ~BatchNormRewriter() = default;
  tensorflow::StringPiece name() const override { return "batchnorm_rewriter"; }

  // Run operation expander on the given computation. Returns whether the
  // computation was changed.
  StatusOr<bool> Run(HloModule* module) override;

 private:
  bool rewrite_training_op_;
  bool rewrite_grad_op_;
  bool use_fusion_;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_BATCHNORM_REWRITER_H_
