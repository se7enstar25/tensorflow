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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_MEMORY_SPACE_ASSIGNMENT_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_MEMORY_SPACE_ASSIGNMENT_H_

#include "tensorflow/compiler/xla/service/heap_simulator.h"
#include "tensorflow/compiler/xla/service/hlo_cost_analysis.h"

namespace xla {

// This class contains pre-set assignments determined by memory space
// assignment. It contains two data structures: (1) a chunks vector that maps a
// defining HloPosition to a Chunk (offset and size), and (2) a sizes vector
// that maps the memory space to its size. If there is only one alternate memory
// space like there is currently, there will be one entry in sizes.
class PresetAssignments {
 public:
  PresetAssignments() = default;

  void add_chunk(const HloPosition& position,
                 const HeapSimulator::Chunk& chunk) {
    chunks_.emplace_back(position, chunk);
  }

  void add_size(int64 memory_space, int64 size) {
    sizes_.emplace_back(memory_space, size);
  }

  absl::Span<const std::pair<HloPosition, HeapSimulator::Chunk>> chunks()
      const {
    return chunks_;
  }

  absl::Span<const std::pair<int64, int64>> sizes() const { return sizes_; }

  // Remove the chunks_ entry that corresponds to instruction.
  void RemoveAssignmentForInstruction(const HloInstruction* instruction);

 private:
  std::vector<std::pair<HloPosition, HeapSimulator::Chunk>> chunks_;
  std::vector<std::pair<int64, int64>> sizes_;
};

// A wrapper class around HloCostAnalysis with additional knowledge about the
// bandwidths of different memory spaces.
class MemorySpaceAssignmentCostAnalysis {
 public:
  MemorySpaceAssignmentCostAnalysis(
      const HloCostAnalysis& cost_analysis,
      float async_copy_bandwidth_bytes_per_second,
      float alternate_mem_bandwidth_bytes_per_second)
      : cost_analysis_(cost_analysis),
        async_copy_bandwidth_bytes_per_second_(
            async_copy_bandwidth_bytes_per_second),
        alternate_mem_bandwidth_bytes_per_second_(
            alternate_mem_bandwidth_bytes_per_second) {}

  const HloCostAnalysis& cost_analysis() const { return cost_analysis_; }

  // Returns the elapsed time in seconds due to compute only.
  float GetInstructionElapsedDueToCompute(
      const HloInstruction& instruction) const;

  // Returns the elapsed time in seconds due to memory only. If
  // operand_in_alternate_mem is provided or if output_in_alternate_mem is true,
  // it will assume that operand or output will be in the alternate memory
  // space. This is useful for calculating the benefit of placing the buffer in
  // alternate memory.
  float GetInstructionElapsedDueToMemory(
      const HloInstruction& instruction,
      absl::optional<int64> operand_in_alternate_mem = absl::nullopt,
      bool output_in_alternate_mem = false) const;

  // Returns the estimated elapsed duration of the instruction in seconds.  It
  // assumes all operands and outputs of the instruction are in the default
  // memory, except for the operand number that is in the alternate memory, if
  // provided, or output if output_in_alternate_mem is true.
  float GetInstructionElapsed(
      const HloInstruction& instruction,
      absl::optional<int64> operand_in_alternate_mem = absl::nullopt,
      bool output_in_alternate_mem = false) const;

  // Returns the elapsed time it would take to asynchronously copy the shape
  // from default to alternate memory space (or vice versa).
  float GetAsyncCopyElapsed(const Shape& shape) const;

 private:
  const HloCostAnalysis& cost_analysis_;
  float async_copy_bandwidth_bytes_per_second_;
  float alternate_mem_bandwidth_bytes_per_second_;
};

// Abstract base class that memory space assignment uses to pick prefetch
// intervals.
class PrefetchIntervalPicker {
 public:
  PrefetchIntervalPicker() = default;
  virtual ~PrefetchIntervalPicker() = default;

  // Sets the instruction schedule.
  virtual void SetInstructionSchedule(
      const absl::flat_hash_map<const HloInstruction*, int64>&
          instruction_schedule) {
    instruction_schedule_ = &instruction_schedule;
  }

  // Returns true if the buffer can be allocated in alternate memory space
  // without any copies (prefetches).
  virtual bool CanAllocateInAlternateMemoryNoCopy(const Shape& shape,
                                                  int64 start_time,
                                                  int64 end_time) const = 0;

  // Begins the iterator for the first start time of the prefetch.
  virtual void Begin(const HloUse& use, int64 start_time, int64 end_time) = 0;

  // Advances the start time of the prefetch and returns that value.
  virtual int64 Next() = 0;

  // Returns true if the available prefetch intervals have been exhausted.
  virtual bool Done() const = 0;

 protected:
  const absl::flat_hash_map<const HloInstruction*, int64>*
      instruction_schedule_ = nullptr;
};

// Prefetch interval picker that uses instruction count to overlap asynchronous
// copies with independent computation. The min and max overlap counts describe
// the number of independent HLOs overlapped while a value is being prefetched
// into the alternate memory (between CopyStart and CopyDone HLO instructions).
// max_overlap_count attempts to prevent bringing tensors into the alternate
// memory too eagerly and hence occupying the space for other tensors which
// might use it.  min_overlap_count attempts to prevent cases where tensors are
// prefetched into the alternate memory without sufficient time for the copy to
// take place.  In those cases, it's just better to keep the tensor in the
// default memory instead of hurting the critical path with this copy that
// likely won't finish in time.
class InstructionCountPrefetchIntervalPicker : public PrefetchIntervalPicker {
 public:
  InstructionCountPrefetchIntervalPicker(int64 min_overlap_count,
                                         int64 max_overlap_count)
      : min_overlap_count_(min_overlap_count),
        max_overlap_count_(max_overlap_count) {}

  bool CanAllocateInAlternateMemoryNoCopy(const Shape& shape, int64 start_time,
                                          int64 end_time) const override;

  void Begin(const HloUse& use, int64 start_time, int64 end_time) override;

  int64 Next() override;
  bool Done() const override;

 private:
  int64 min_overlap_count_;
  int64 max_overlap_count_;
  int64 end_time_;
  int64 current_prefetch_time_;
};

// Prefetch interval picker that uses cost analysis to overlap asynchronous
// copies with independent computation. It uses min/max (asynchronous copy
// duration) / (independent computation duration) ratios to guide whether the
// prefetch is within those bounds. It starts with the maximum allowed ratio
// (earliest prefetch) in Begin() and works its way for later and later prefetch
// with each Next() call until hitting the minimum ratio, in order not to hurt
// the critical path.
class CostAnalysisPrefetchIntervalPicker : public PrefetchIntervalPicker {
 public:
  CostAnalysisPrefetchIntervalPicker(
      const MemorySpaceAssignmentCostAnalysis& cost_analysis,
      float min_async_copy_to_overlap_ratio,
      float max_async_copy_to_overlap_ratio)
      : cost_analysis_(cost_analysis),
        min_async_copy_to_overlap_ratio_(min_async_copy_to_overlap_ratio),
        max_async_copy_to_overlap_ratio_(max_async_copy_to_overlap_ratio) {}

  void SetInstructionSchedule(
      const absl::flat_hash_map<const HloInstruction*, int64>&
          instruction_schedule) override;

  bool CanAllocateInAlternateMemoryNoCopy(const Shape& shape, int64 start_time,
                                          int64 end_time) const override;

  void Begin(const HloUse& use, int64 start_time, int64 end_time) override;

  int64 Next() override;
  bool Done() const override;

 private:
  // Returns the elapsed time in seconds between the logical interval that
  // corresponds to the instruction schedule.
  float GetLogicalIntervalElapsed(int64 start_time, int64 end_time) const;

  // For performance reasons, we calculate the prefix sum of the elapsed time so
  // that it's efficient to find the elapsed time in seconds in any logical
  // interval.
  std::vector<float> elapsed_time_cumsum_;

  const MemorySpaceAssignmentCostAnalysis& cost_analysis_;
  float min_async_copy_to_overlap_ratio_;
  float max_async_copy_to_overlap_ratio_;

  float async_copy_elapsed_;
  float inst_elapsed_reduction_;
  int64 end_logical_time_;
  int64 current_logical_prefetch_time_;
};

// MemorySpaceAssignment assigns memory spaces (default or alternate) to each
// instruction in the module. It will greedily try placing as as many values in
// the alternate memory space as possible. It uses the heap simulator to
// determine the actual allocation offsets of values in the alternate memory
// space to account for fragmentation. The default memory space is assumed to be
// large enough to hold the values that could not be placed in the alternate
// memory space.
class MemorySpaceAssignment {
 public:
  using Chunk = HeapSimulator::Chunk;
  using BufferInterval = GlobalDecreasingSizeBestFitHeap::BufferInterval;
  using BufferIntervalCompare =
      GlobalDecreasingSizeBestFitHeap::BufferIntervalCompare;

  // MemorySpaceAssignment uses a notion of a slow and large default memory
  // space and a fast and small alternate memory space.
  enum class MemorySpace { kDefault, kAlternate };

  // This class represents an allocation that might either be in the default or
  // alternate memory. An HloValue might live in multiple different allocations
  // over its lifetime. The lifetimes of the allocations are defined using
  // start_time and end_time, which corresponds to the instruction indexes in
  // the flattened schedule. Each of these allocations might partially overlap
  // with each other. CopyAllocation defined below represents asynchronous
  // copies between Allocations.
  //
  // Consider an instruction Foo, and its users Bar and Baz, and the times given
  // in terms of the flattened schedule of the entire module:
  //
  //      Foo:10
  //       /   \
  //    Bar:14  \
  //           Baz:25
  //
  // A valid memory space assignment could be like the following:
  //
  //  Time:         10 ... 14        ...      25
  //                Foo    Bar                Baz
  //  Alternate     +-------+           +-----+
  //  Default           +---------------------+
  //                    ^   ^           ^     ^
  //                    |   |           |     |
  //                evict   evict  prefetch  prefetch
  //                start    end    start      end
  //
  // This would be represented with:
  //   - Allocation(memory_space=kAlternate, start_time=10, end_time=14)
  //   - CopyAllocation(memory_space=kDefault, start_time=12, end_time=25)
  //   - CopyAllocation(memory_space=kAlternate, start_time=22, end_time=25)
  class Allocation {
   public:
    Allocation(HloInstruction* instruction, HloPosition defining_position,
               MemorySpace memory_space, Chunk chunk, int64 start_time,
               int64 end_time)
        : instruction_(instruction),
          defining_position_(defining_position),
          memory_space_(memory_space),
          chunk_(chunk),
          start_time_(start_time),
          end_time_(end_time) {}
    virtual ~Allocation() = default;

    virtual bool is_copy_allocation() const { return false; }

    // Adds a use to this allocation.
    void AddUse(HloUse use);

    // Extends the end time of this allocation.
    void Extend(int64 end_time) { end_time_ = end_time; }

    // After all of the time ranges for the allocations have been assigned,
    // Process morphs the instructions affected to assign the memory spaces and
    // insert asynchronous copy instructions if necessary.
    virtual Status Process(MemorySpaceAssignment* memory_space_assignment);

    // Returns the instruction that produces this allocation. It might be
    // different than the instruction in defining_position (e.g., a
    // GetTupleElement instruction does not define the buffer).
    virtual HloInstruction* instruction() const { return instruction_; }

    // Returns the defining position for this allocation.
    virtual HloPosition defining_position() const { return defining_position_; }

    const std::vector<HloUse>& uses() const { return uses_; }
    MemorySpace memory_space() const { return memory_space_; }
    Chunk chunk() const { return chunk_; }
    void set_start_time(int64 start_time) { start_time_ = start_time; }
    int64 start_time() const { return start_time_; }
    int64 end_time() const { return end_time_; }

   protected:
    // Descend to the shape_index element of the tuple and replace that with
    // new_instruction.
    StatusOr<HloInstruction*> ReplaceTupleWith(HloInstruction* new_instruction,
                                               HloInstruction* tuple,
                                               ShapeIndex shape_index);

    HloInstruction* instruction_;
    HloPosition defining_position_;
    std::vector<HloUse> uses_;
    std::vector<HloInstruction*> bitcasts_;
    MemorySpace memory_space_;
    Chunk chunk_;
    int64 start_time_;
    int64 end_time_;
  };

  // This class represents an allocation as a result of an asynchronous copy.
  class CopyAllocation : public Allocation {
   public:
    CopyAllocation(const Allocation& prev_allocation, MemorySpace memory_space,
                   Chunk chunk, int64 start_time, int64 end_time)
        : Allocation(/*instruction=*/nullptr,
                     /*defining_position=*/{nullptr, {}}, memory_space, chunk,
                     start_time, end_time),
          prev_allocation_(prev_allocation),
          copy_start_schedule_after_(start_time),
          copy_done_schedule_before_(end_time) {}

    bool is_copy_allocation() const override { return true; }

    Status Process(MemorySpaceAssignment* memory_space_assignment) override;

    HloInstruction* instruction() const override {
      // Unless explicitly set, the instruction of a copy allocation in
      // retrieved from the previous allocation.
      if (instruction_ != nullptr) {
        return instruction_;
      } else {
        return prev_allocation_.instruction();
      }
    }

    HloPosition defining_position() const override {
      // Unless explicitly set, the defining position of a copy allocation in
      // retrieved from the previous allocation. This is because we don't create
      // new CopyStart/CopyDone instructions until later and the position should
      // point to the previous (copy or otherwise) allocation's position for the
      // original defining position.
      if (defining_position_.instruction == nullptr) {
        return prev_allocation_.defining_position();
      } else {
        return defining_position_;
      }
    }

    HloInstruction* copy_start() const { return copy_start_; }
    HloInstruction* copy_done() const { return copy_done_; }

    int64 copy_start_schedule_after() const {
      return copy_start_schedule_after_;
    }
    int64 copy_done_schedule_before() const {
      return copy_done_schedule_before_;
    }

    void set_copy_start_schedule_after(int64 copy_start_schedule_after) {
      copy_start_schedule_after_ = copy_start_schedule_after;
    }

   private:
    const Allocation& prev_allocation_;
    // These variables define the scheduling boundaries where CopyStart and
    // CopyDone can be scheduled. The earliest CopyStart can be scheduled is
    // after copy_start_schedule_after_ and the latest CopyDone can be scheduled
    // is before copy_done_schedule_before_.
    int64 copy_start_schedule_after_;
    int64 copy_done_schedule_before_;
    HloInstruction* copy_start_;
    HloInstruction* copy_done_;
  };

  using AllocationSequence = std::list<std::unique_ptr<Allocation>>;
  using AllocationMap =
      absl::flat_hash_map<const HloValue*, AllocationSequence>;

  // Runs the MemorySpaceAssignment pass. alternate_memory_space is the
  // architecture-specific integer value that describes the alternate memory.
  // max_size_in_bytes is the maximum size of the alternate memory.
  // If a buffer_interval_compare is provided, we sort the buffers using that
  // (otherwise, we use GlobalDecreasingSizeBestFitHeap::kSpatial).
  // prefetch_interval_picker determines how early and how late can prefetches
  // occur. alternate_memory_space_alignment_in_bytes is the alignment required
  // in the alternate memory space, size_fn is the size function for buffer
  // values, and is_allowed_in_alternate_mem can be used to prevent certain
  // HloValues (e.g., based on the opcode) to be placed on the alternate memory.
  // max_outstanding_async_copies specifies the upper bound for number of
  // outstanding asynchronous copies, -1 for unlimited.
  static StatusOr<std::unique_ptr<PresetAssignments>> Run(
      HloModule* module, int64 alternate_memory_space, int64 max_size_in_bytes,
      absl::optional<BufferIntervalCompare> buffer_interval_compare,
      PrefetchIntervalPicker* prefetch_interval_picker,
      int64 alternate_memory_space_alignment_in_bytes,
      BufferValue::SizeFunction size_fn,
      std::function<bool(const HloValue&)> is_allowed_in_alternate_mem,
      int64 max_outstanding_async_copies = -1);

  // Returns the maximum number of outstanding asynchronous copies in the
  // module.
  static int64 CountMaximumOutstandingAsyncCopies(const HloModule& module);

  static BufferIntervalCompare GetMemoryBoundednessBufferIntervalCompare(
      const MemorySpaceAssignmentCostAnalysis& cost_analysis);

 private:
  MemorySpaceAssignment(HloModule* module, int64 alternate_memory_space)
      : module_(module),
        alternate_memory_space_(alternate_memory_space),
        preset_assignments_(absl::make_unique<PresetAssignments>()) {}

  // Process calls Process methods of the allocations after the allocations have
  // been finalized.
  Status Process();

  // Process() might have altered the computation graph by inserting kTuple and
  // kGetTupleElement instructions. SimplifyGraph performs a simple DCE and
  // tuple simplification operation (e.g., given GetTupleElement(Tuple(a, b),
  // 1), simply forwards b). Runs to fixed point.
  Status SimplifyGraph();

  // FixSchedule inserts asynchronous copies in the schedule.
  Status FixSchedule();

  // Insert an instruction to the schedule, and make sure its dependencies
  // (operands) are already in the schedule. If not, insert these operands
  // before the instruction.
  void EnsureInstructionAndOperandsInserted(
      HloInstruction* new_instruction, HloInstructionSequence* new_sequence,
      absl::flat_hash_set<HloInstruction*>* inserted_instructions) const;

  // Schedules asynchronous copies and ensures that the CopyStarts and their
  // corresponding CopyDones follow the same order.
  void ScheduleAsynchronousCopies();

  HloModule* module_;
  int64 alternate_memory_space_;
  std::unique_ptr<HloLiveRange> hlo_live_range_;
  AllocationMap allocation_map_;
  std::unique_ptr<PresetAssignments> preset_assignments_;

  // These maps hold vectors of new instructions that need to be scheduled after
  // (or before) the instruction index in the key. FixSchedule uses these maps
  // to modify and fix the schedule.
  absl::flat_hash_map<int64, std::vector<HloInstruction*>> schedule_after_;
  absl::flat_hash_map<int64, std::vector<HloInstruction*>> schedule_before_;
};

// This struct contains mandatory memory assignments at a given time. E.g., an
// input's required memory assignment time would correspond to the definition
// time of the parameter instruction, and an output's time would correspnd to
// the time of last use.
struct RequiredMemoryAssignment {
  MemorySpaceAssignment::MemorySpace memory_space;
  int64 time;
};

// This class inherits from GlobalDecreasingSizeBestFitHeap with a notion of
// maximum size.
class AlternateMemoryBestFitHeap : public GlobalDecreasingSizeBestFitHeap {
 public:
  using IsAllowedInAlternateMemoryFunction =
      std::function<bool(const HloValue&)>;
  using MemorySpace = MemorySpaceAssignment::MemorySpace;

  AlternateMemoryBestFitHeap(
      MemorySpaceAssignment::AllocationMap* allocation_map,
      int64 max_size_in_bytes,
      absl::optional<BufferIntervalCompare> buffer_interval_compare,
      PrefetchIntervalPicker* prefetch_interval_picker,
      const HloAliasAnalysis& alias_analysis,
      const HloLiveRange& hlo_live_range, int64 alignment,
      IsAllowedInAlternateMemoryFunction is_allowed_in_alternate_mem,
      int64 max_outstanding_async_copies)
      : GlobalDecreasingSizeBestFitHeap(alignment),
        allocation_map_(allocation_map),
        max_size_in_bytes_(max_size_in_bytes),
        prefetch_interval_picker_(prefetch_interval_picker),
        alias_analysis_(alias_analysis),
        hlo_live_range_(hlo_live_range),
        is_allowed_in_alternate_mem_(is_allowed_in_alternate_mem),
        max_outstanding_async_copies_(max_outstanding_async_copies) {
    // Override buffer interval compare if provided.
    if (buffer_interval_compare) {
      buffer_interval_compare_ = *buffer_interval_compare;
    }
  }

  HeapSimulator::Result Finish() override;

 private:
  // Finds an allocation for the given interval. Internally, it will attempt to
  // find a suitable chunk candidate within the heap size and prefetch interval
  // limits, and append the new allocation(s) to allocations. The new
  // allocations can be in default or alternate memory spaces, or can be
  // prefetches or evictions. Returns true if successful.
  bool FindAllocation(int64 start_time, int64 end_time, int64 last_use_time,
                      HloPosition defining_position, HloUse use,
                      const HloValue* buffer, int64 size,
                      MemorySpaceAssignment::AllocationSequence* allocations);

  // Try allocating in alternate memory without any copies. Returns true if
  // successful.
  bool TryAllocatingInAlternateMemoryNoCopy(
      int64 start_time, int64 end_time, int64 last_use_time,
      HloPosition defining_position, HloUse use,
      BufferInterval alternate_mem_interval,
      HloInstruction* non_bitcast_operand,
      MemorySpaceAssignment::AllocationSequence* allocations);

  // Adds input and outputs as required assignments.
  void AddInputAndOutputRequiredAssignments();

  // Given a buffer interval, returns the colocated intervals. Unlike the
  // similar GlobalDecreasingSizeBestFitHeap::GetTransitiveColocations, it
  // returns the colocated intervals sorted by scheduled time.
  std::vector<const BufferInterval*> GetSortedColocatedIntervals(
      const BufferInterval& interval) const;

  // Since the allocations are recorded to the AllocationMap, we don't maintain
  // result_ in GlobalDecreasingSizeBestFitHeap. Override AddToChunkMap to avoid
  // unnecessarily adding the chunk to the chunk map.
  void AddToChunkMap(const HloValue* buffer, Chunk chunk) override {}

  // Returns true if the addition of an asynchronous copy in the given time
  // interval would violate the maximum number of asynchronous copies.
  bool ViolatesMaximumOutstandingAsyncCopies(int64 start_time,
                                             int64 end_time) const;

  // Adds an asynchronous copy to the allocations.
  void AddAsyncCopy(const MemorySpaceAssignment::Allocation& prev_allocation,
                    MemorySpace memory_space, Chunk chunk, int64 start_time,
                    int64 end_time,
                    MemorySpaceAssignment::AllocationSequence* allocations);

  // These methods are used for delaying committing the chunk candidate until
  // the entire live range of the buffer has been considered.
  void AddToPendingChunks(const BufferInterval& buffer_interval,
                          const ChunkCandidate& chunk_candidate);
  void CommitPendingChunks();

  MemorySpaceAssignment::AllocationMap* allocation_map_;
  int64 max_size_in_bytes_;
  PrefetchIntervalPicker* prefetch_interval_picker_;
  const HloAliasAnalysis& alias_analysis_;
  const HloLiveRange& hlo_live_range_;
  IsAllowedInAlternateMemoryFunction is_allowed_in_alternate_mem_;
  // We use a interval tree to keep track of the number of outstanding
  // asynchronous copies.
  BufferIntervalTree async_copy_interval_tree_;
  int64 max_outstanding_async_copies_;
  std::vector<std::pair<BufferInterval, ChunkCandidate>> pending_chunks_;
  std::vector<std::pair<int64, int64>> pending_async_copies_;
  // This map contains required memory assignments for HloValues (e.g., input
  // and outputs).
  absl::flat_hash_map<const HloValue*, std::vector<RequiredMemoryAssignment>>
      required_assignments_;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_MEMORY_SPACE_ASSIGNMENT_H_
