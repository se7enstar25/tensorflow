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

#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/Pass/PassManager.h"  // from @llvm-project
#include "mlir/Transforms/Passes.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_device.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/passes.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/attribute_utils.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/tpu_cluster_util.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/tpu_rewrite_device_util.h"

namespace mlir {
namespace TFTPU {

namespace {

constexpr char kDeviceAttr[] = "device";
constexpr char kClassAttr[] = "_class";

#define GEN_PASS_DEF_TPUCLEANUPCLUSTERATTRIBUTESPASS
#include "tensorflow/compiler/mlir/tensorflow/transforms/tf_passes.h.inc"

class TPUCleanupClusterAttributesPass
    : public impl::TPUCleanupClusterAttributesPassBase<
          TPUCleanupClusterAttributesPass> {
 public:
  void runOnOperation() override {
    auto traverse_op = [&](Operation* op, tf_device::ClusterOp tpu_cluster) {
      if (isa<tf_device::ClusterOp>(op)) return WalkResult::advance();
      op->removeAttr(TF::kReplicationInfoAttr);
      op->removeAttr(TF::kCompileDeviceTypeAttr);
      // This attribute is used for op colocation. Since all ops are located
      // on a single device cluster, this private attribute is no longer
      // needed.
      op->removeAttr(kClassAttr);
      if (auto attr = op->getAttrOfType<StringAttr>(kDeviceAttr)) {
        // Preserve device attribute if the op is placed on a replicated core
        // device. Device attribute is used to infer the appropriate sharding
        // within TPUs for this op.
        // TODO(b/183598857): Use explicit sharding ops from the front-end.
        // For example, dequeue ops generated by
        // tensorflow/python/tpu/tpu_feed.py
        if (!tensorflow::IsTPUReplicatedCore(attr.getValue()) &&
            !isa<tf_device::LaunchOp>(op)) {
          op->removeAttr(kDeviceAttr);
        }
      }
      return WalkResult::advance();
    };

    if (failed(TFTPU::WalkReachableFromTpuCluster(getOperation(), traverse_op)))
      return signalPassFailure();
  }
};

}  // namespace

std::unique_ptr<OperationPass<ModuleOp>>
CreateTPUClusterCleanupAttributesPass() {
  return std::make_unique<TPUCleanupClusterAttributesPass>();
}

}  // namespace TFTPU
}  // namespace mlir
