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

#include "tensorflow/core/ir/interfaces.h"

#include "tensorflow/core/ir/types/dialect.h"

namespace mlir {
namespace tfg {
LogicalResult ControlArgumentInterface::verifyRegion(Operation *op,
                                                     Region &region) {
  unsigned num_ctl = 0, num_data = 0;
  for (BlockArgument arg : region.getArguments()) {
    bool is_ctl = arg.getType().isa<tf_type::ControlType>();
    num_ctl += is_ctl;
    num_data += !is_ctl;
  }
  if (num_ctl != num_data) {
    return op->emitOpError("region #")
           << region.getRegionNumber()
           << " expected same number of data values and control tokens ("
           << num_data << " vs. " << num_ctl << ")";
  }
  return success();
}
}  // namespace tfg
}  // namespace mlir

// Include the generated definitions.
#include "tensorflow/core/ir/interfaces.cc.inc"
