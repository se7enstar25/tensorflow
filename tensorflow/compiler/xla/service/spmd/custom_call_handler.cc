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

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "tensorflow/compiler/xla/client/lib/comparators.h"
#include "tensorflow/compiler/xla/client/xla_builder.h"
#include "tensorflow/compiler/xla/literal_util.h"
#include "tensorflow/compiler/xla/service/hlo_casting_utils.h"
#include "tensorflow/compiler/xla/service/hlo_computation.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/hlo_instructions.h"
#include "tensorflow/compiler/xla/service/hlo_sharding.h"
#include "tensorflow/compiler/xla/service/hlo_sharding_util.h"
#include "tensorflow/compiler/xla/service/shape_inference.h"
#include "tensorflow/compiler/xla/service/spmd/spmd_partitioner.h"
#include "tensorflow/compiler/xla/service/spmd/spmd_partitioner_util.h"
#include "tensorflow/compiler/xla/shape_util.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/compiler/xla/window_util.h"
#include "tensorflow/core/platform/numbers.h"

namespace xla {
namespace spmd {

Status SpmdPartitioningVisitor::HandleCustomCallTopK(HloInstruction* hlo) {
  if (!hlo->operand(0)->has_sharding()) {
    return DefaultAction(hlo);
  }

  const HloSharding& sharding = hlo->operand(0)->sharding();
  // No support for partial replicate yet.
  if (sharding.IsTileMaximal() || sharding.IsReplicated() ||
      sharding.ReplicateOnLastTileDim()) {
    return DefaultAction(hlo);
  }

  const int64 batch_dim = 0;
  const int64 sort_dim = 1;
  const int64 shard_count = sharding.tile_assignment().dim(sort_dim);

  if (shard_count <= 1) {
    return DefaultAction(hlo);
  }

  const int64 batch_dim_partition = sharding.tile_assignment().dim(batch_dim);
  const int64 input_size = hlo->operand(0)->shape().dimensions(sort_dim);
  const int64 batch_size = hlo->shape().tuple_shapes(0).dimensions(batch_dim);
  const int64 k = hlo->shape().tuple_shapes(0).dimensions(sort_dim);
  const int64 per_partition_size = CeilOfRatio(input_size, shard_count);

  if (k >= per_partition_size) {
    return DefaultAction(hlo);
  }

  auto input = hlo->operand(0);
  const auto element_type = input->shape().element_type();

  auto partitioned_input = GetPartitionedHlo(input).PadWithValue(
      CreateFirstWithType(element_type, &b_));

  auto partition_state = partitioned_input.state();
  auto replicated_sharding = HloSharding::Replicate();
  // If batch dimension is partitioned, partial replicated on sort dimension.
  if (batch_dim_partition > 1) {
    auto sharding_grouped = GroupShardingOnDims(sharding, {batch_dim});
    partition_state = CreatePerGroupPartitioningState(
        partitioned_input.state(), sharding_grouped.device_groups,
        partitioned_input.state().b);
    auto reshape_tile_assignment = sharding.tile_assignment();
    auto reshape_dimensions = reshape_tile_assignment.dimensions();
    reshape_dimensions.push_back(reshape_dimensions.back());
    reshape_dimensions[sort_dim] = 1;
    reshape_tile_assignment.Reshape(reshape_dimensions);
    replicated_sharding = HloSharding::PartialTile(reshape_tile_assignment);
  }

  // Each partition needs to do TopK separately, thus the base shape
  // becomes [batch_size, k * shard_count].
  const Shape replicated_shape = ShapeUtil::MakeTupleShape(
      {ShapeUtil::MakeShape(hlo->operand(0)->shape().element_type(),
                            {batch_size, k * shard_count}),
       ShapeUtil::MakeShape(S32, {batch_size, k * shard_count})});
  auto custom_call_sharding =
      sharding.GetTupleSharding(replicated_shape).ValueOrDie();
  auto shard_shape =
      MakePartitionedShape(replicated_shape, custom_call_sharding);
  auto topk = b_.AddInstruction(
      hlo->CloneWithNewOperands(shard_shape, {partitioned_input.hlo()}));
  topk->set_sharding(custom_call_sharding);
  // Partition customcall.
  PartitionedHlo partitioned_topk(topk, replicated_shape,
                                  MakePartitioningState());
  topk = partitioned_topk.hlo();

  // Get value from TopK.
  HloInstruction* value_gte =
      b_.AddInstruction(HloInstruction::CreateGetTupleElement(
          topk->shape().tuple_shapes(0), topk, 0));
  value_gte->set_sharding(sharding);
  // Partition GetTupleElement of value.
  PartitionedHlo value_partitioned_gte(
      value_gte, partitioned_topk.base_shape().tuple_shapes(0),
      MakePartitioningState());
  // Reshard value to be replicated.
  auto replicated_value_gte =
      value_partitioned_gte.Reshard(replicated_sharding).hlo();

  // Get index from TopK.
  HloInstruction* index_gte =
      b_.AddInstruction(HloInstruction::CreateGetTupleElement(
          topk->shape().tuple_shapes(1), topk, 1));
  auto partition_id_s32 = b_.AddInstruction(HloInstruction::CreateConvert(
      ShapeUtil::MakeShape(S32, partition_id_->shape().dimensions()),
      partition_state.partition_id));
  // Add per partition offset to index, index returned from CustomCall always
  // starts from 0.
  auto index_offset = b_.AddInstruction(HloInstruction::CreateBroadcast(
      index_gte->shape(),
      b_.AddInstruction(HloInstruction::CreateBinary(
          partition_id_s32->shape(), HloOpcode::kMultiply, partition_id_s32,
          b_.AddInstruction(HloInstruction::CreateConstant(
              LiteralUtil::CreateR0<int32>(per_partition_size))))),
      {}));
  index_gte = b_.AddInstruction(HloInstruction::CreateBinary(
      index_offset->shape(), HloOpcode::kAdd, index_gte, index_offset));
  index_gte->set_sharding(sharding);
  // Parttion GetTupleElement of index.
  PartitionedHlo index_partitioned_gte(
      index_gte, partitioned_topk.base_shape().tuple_shapes(1),
      MakePartitioningState());
  // Reshard index to be replicated.
  auto replicated_index_gte =
      index_partitioned_gte.Reshard(replicated_sharding).hlo();

  // Creates replicated sort to do TopK, the input is value and index pairs
  // from all the partitions. The reason to use Sort instead of CustomCall TopK
  // is CustomCall only takes value as input. There will be an extra Gather
  // to get the correct index if CustomCall is used here.

  // Create comparator for the sort.
  XlaBuilder b("Sort.Compare");
  XlaComputation comparator = CreateScalarComparisonComputation(
      "compare-value-and-index", {input->shape().element_type(), S32}, {Gt, Lt},
      &b);
  TF_ASSIGN_OR_RETURN(ProgramShape program_shape, comparator.GetProgramShape());
  HloModuleConfig config(program_shape);
  TF_ASSIGN_OR_RETURN(auto new_module,
                      HloModule::CreateFromProto(comparator.proto(), config));
  HloCloneContext context(module_);
  auto compare_computation =
      module_->DeepCloneComputation(new_module->entry_computation(), &context);
  // Each partition needs to do TopK separately, thus the base shape for sort
  // becomes [ceil(batch_size / batch_dim_partition), k * shard_count].
  const Shape sort_shape = ShapeUtil::MakeTupleShape(
      {ShapeUtil::MakeShape(
           hlo->operand(0)->shape().element_type(),
           {CeilOfRatio(batch_size, batch_dim_partition), k * shard_count}),
       ShapeUtil::MakeShape(S32, {CeilOfRatio(batch_size, batch_dim_partition),
                                  k * shard_count})});
  auto sort = b_.AddInstruction(HloInstruction::CreateSort(
      sort_shape, sort_dim, {replicated_value_gte, replicated_index_gte},
      compare_computation, true));
  sort->set_sharding(
      replicated_sharding.GetTupleSharding(sort->shape()).ValueOrDie());
  PartitionedHlo replicated_sort(sort, replicated_shape,
                                 MakePartitioningState());

  // Slice value and index from top-k for output.
  HloInstruction* sort_value_gte =
      b_.AddInstruction(HloInstruction::CreateGetTupleElement(
          replicated_sort.hlo()->shape().tuple_shapes(0), replicated_sort.hlo(),
          0));
  HloInstruction* sort_index_gte =
      b_.AddInstruction(HloInstruction::CreateGetTupleElement(
          replicated_sort.hlo()->shape().tuple_shapes(1), replicated_sort.hlo(),
          1));
  // Slice value from final sort.
  HloInstruction* slice_sort_value =
      SliceFirstK(sort_value_gte, &b_, sort_dim, k);
  // Slice index from final sort.
  HloInstruction* slice_index_value =
      SliceFirstK(sort_index_gte, &b_, sort_dim, k);
  auto create_tuple = b_.AddInstruction(
      HloInstruction::CreateTuple({slice_sort_value, slice_index_value}));
  create_tuple->set_sharding(
      replicated_sharding.GetTupleSharding(create_tuple->shape()).ValueOrDie());
  SetPartitionedHlo(
      hlo, PartitionedHlo(create_tuple, hlo->shape(), MakePartitioningState())
               .Reshard(hlo->sharding()));

  return Status::OK();
}

Status SpmdPartitioningVisitor::HandleCustomCall(HloInstruction* hlo) {
  if (hlo->custom_call_target() == "SPMDFullToShardShape") {
    // This op switches from auto partitioning to manual partitioning.
    auto input_partitioned = GetPartitionedHlo(hlo->operand(0));
    if (!EvenlyPartitions(hlo->shape(), input_partitioned.sharding())) {
      input_partitioned = input_partitioned.PadWithValue(
          CreateR0WithType(hlo->shape().element_type(), 0, &b_));
    }
    auto input = input_partitioned.hlo();
    CHECK(hlo->sharding().IsManual());
    CHECK(ShapeUtil::Compatible(input->shape(), hlo->shape()));
    auto copy = b_.AddInstruction(
        HloInstruction::CreateUnary(input->shape(), HloOpcode::kCopy, input));
    SetPartitionedHlo(hlo, [&] { return copy; });
    return Status::OK();
  }
  if (hlo->custom_call_target() == "SPMDShardToFullShape") {
    // This op switches from manual partitioning to auto partitioning.
    auto input = GetPartitionedHlo(hlo->operand(0)).hlo();
    CHECK(input->sharding().IsManual());
    auto copy = b_.AddInstruction(
        HloInstruction::CreateUnary(input->shape(), HloOpcode::kCopy, input));
    CHECK(ShapeUtil::Compatible(
        copy->shape(), MakePartitionedShape(hlo->shape(), hlo->sharding())));
    SetPartitionedHlo(hlo, [&] { return copy; });
    return Status::OK();
  }

  if (hlo->custom_call_target() == "TopK") {
    return HandleCustomCallTopK(hlo);
  }

  return DefaultAction(hlo);
}

}  // namespace spmd
}  // namespace xla
