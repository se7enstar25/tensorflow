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

#include "tensorflow/lite/delegates/gpu/gl/kernels/space_to_depth.h"

#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/types/any.h"
#include "tensorflow/lite/delegates/gpu/common/operations.h"
#include "tensorflow/lite/delegates/gpu/common/status.h"
#include "tensorflow/lite/delegates/gpu/gl/node_shader.h"

namespace tflite {
namespace gpu {
namespace gl {
namespace {

class SpaceToDepth : public NodeShader {
 public:
  Status GenerateCode(const GenerationContext& ctx,
                      GeneratedCode* generated_code) const final {
    const auto attr =
        absl::any_cast<SpaceToDepthAttributes>(ctx.node->operation.attributes);
    const auto& input_data_0 = ctx.graph->FindInputs(ctx.node->id)[0]->tensor;
    std::string code = R"(
      for (int i = 0; i < 4; ++i) {
        int dst_c = 4 * gid.z + i;
        int block_id = dst_c / $input_data_0_c$;
        int src_x = gid.x * $block_size$ + block_id % $block_size$;
        int src_y = gid.y * $block_size$ + block_id / $block_size$;
        int src_c = dst_c % $input_data_0_c$;
        value_0[i] = $input_data_0[src_x, src_y, src_c / 4]$[src_c % 4];
      }
    )";

    *generated_code = {
        /*parameters=*/{
            {"block_size", attr.block_size},
            {"input_data_0_c", input_data_0.shape.c},
        },
        /*objects=*/{},
        /*shared_variables=*/{},
        /*workload=*/uint3(),
        /*workgroup=*/uint3(),
        /*source_code=*/std::move(code),
        /*input=*/IOStructure::ONLY_DEFINITIONS,
        /*output=*/IOStructure::AUTO,
    };
    return OkStatus();
  }
};
}  // namespace

std::unique_ptr<NodeShader> NewSpaceToDepthNodeShader() {
  return absl::make_unique<SpaceToDepth>();
}

}  // namespace gl
}  // namespace gpu
}  // namespace tflite
