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

#include "tensorflow/compiler/xla/service/gpu/outfeed_thunk.h"

#include "tensorflow/compiler/xla/literal.h"
#include "tensorflow/compiler/xla/service/gpu/hlo_execution_profiler.h"
#include "tensorflow/compiler/xla/service/gpu/outfeed_manager.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/core/platform/stream_executor_no_cuda.h"

namespace xla {
namespace gpu {

OutfeedConfig GetOutfeedConfig(const HloInstruction* instr) {
  OutfeedConfig config;
  config.input_shape = instr->operand(0)->shape();
  return config;
}

OutfeedThunk::OutfeedThunk(ThunkInfo thunk_info, OutfeedConfig config,
                           std::vector<ShapedSlice> source_slices)
    : Thunk(Kind::kOutfeed, thunk_info),
      config_(std::move(config)),
      source_slices_(std::move(source_slices)) {}

Status OutfeedThunk::ExecuteOnStream(const ExecuteParams& params) {
  auto& stream = *params.stream;
  auto& buffer_allocations = *params.buffer_allocations;

  VLOG(2) << "Outfeeding from GPU";

  // Nothing to be done for empty tuples.
  if (ShapeUtil::IsEmptyTuple(config_.input_shape)) {
    return Status::OK();
  }

  auto op_profiler =
      params.profiler->MakeScopedInstructionProfiler(profile_index());
  OutfeedManager* outfeed_manager = GetOrCreateOutfeedManager();
  ShapeTree<std::unique_ptr<OutfeedBuffer>>* output_buffers =
      outfeed_manager->BlockingGetNextDestination();

  size_t index = 0;
  for (auto& output : output_buffers->leaves()) {
    // Assert that the shapes are compatible.
    const ShapeIndex& shape_index = output.first;
    std::unique_ptr<OutfeedBuffer>& buffer = output.second;
    const Shape& output_shape =
        ShapeUtil::GetSubshape(output_buffers->shape(), shape_index);
    TF_RET_CHECK(ShapeUtil::Equal(source_slices_[index].shape, output_shape))
        << "Mismatch between outfeed output buffer shape "
        << ShapeUtil::HumanStringWithLayout(output_shape)
        << " and outfeed source buffer shape "
        << ShapeUtil::HumanStringWithLayout(source_slices_[index].shape);

    BufferAllocation::Slice source_slice = source_slices_[index++].slice;
    if (!source_slice.allocation())
      return InternalError("outfeed source missing buffer allocation");
    se::DeviceMemoryBase data_address =
        buffer_allocations.GetDeviceAddress(source_slice);

    // TODO(b/111309141): Run this on a separate stream so it doesn't block
    // the GPU from doing work during the transfer. This could be handled by
    // making StreamAssignment do something intelligent with outfeed thunks.
    stream
        .ThenMemcpy(buffer->destination()->untyped_data(), data_address,
                    buffer->length())
        .ThenDoHostCallback([&buffer]() { buffer->Done(); });
  }

  Status block_status = stream.BlockHostUntilDone();
  if (!block_status.ok()) {
    return InternalError("Failed to complete data transfer on stream %p: %s",
                         &stream, block_status.error_message());
  }

  VLOG(2) << "Outfeeding from GPU complete";
  return Status::OK();
}

}  // namespace gpu
}  // namespace xla
