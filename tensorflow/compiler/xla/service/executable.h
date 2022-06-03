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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_EXECUTABLE_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_EXECUTABLE_H_

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "tensorflow/compiler/xla/debug_options_flags.h"
#include "tensorflow/compiler/xla/service/computation_layout.h"
#include "tensorflow/compiler/xla/service/hlo.pb.h"
#include "tensorflow/compiler/xla/service/hlo_execution_profile.h"
#include "tensorflow/compiler/xla/service/hlo_graph_dumper.h"
#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/maybe_owning_device_memory.h"
#include "tensorflow/compiler/xla/service/service_executable_run_options.h"
#include "tensorflow/compiler/xla/service/shaped_buffer.h"
#include "tensorflow/compiler/xla/shape_tree.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"
#include "tensorflow/core/platform/stream_executor_no_cuda.h"
#include "tensorflow/stream_executor/device_memory_allocator.h"

namespace xla {

// TODO(b/150633678): Both the ExecutionInput and ExecutionOutput need to be
// revisited, with the execute APIs taking data structure which can better model
// shareable buffers.
//
// ExecutionInput buffers are in one of three states:
//
// 1) Owned by the caller and immutable.
// 2) Donated by the caller but returned on error.
// 3) Donated by the caller and freed on error.
//
// Case (1) buffers are stored as MaybeOwningDeviceMemory(DeviceMemoryBase).
// Case (2) buffers are stored as MaybeOwningDeviceMemory(OwningDeviceMemory),
//   with their indices present in unowned_indices_.
// Case (3) buffers are stored as MaybeOwningDeviceMemory(OwningDeviceMemory),
//   with their indices absent from unowned_indices_.
class ExecutionInput {
 public:
  explicit ExecutionInput(xla::Shape shape) : buffers_(std::move(shape)) {
    SetHostShape(ShapeUtil::DeviceShapeToHostShape(buffers_.shape()));
  }
  // TODO(b/170310047): remove this overload.
  ExecutionInput(xla::Shape shape, xla::Shape host_shape)
      : buffers_(std::move(shape)) {
    SetHostShape(ShapeUtil::DeviceShapeToHostShape(buffers_.shape()));
  }

  explicit ExecutionInput(ShapeTree<MaybeOwningDeviceMemory> buffers)
      : buffers_(std::move(buffers)) {
    SetHostShape(ShapeUtil::DeviceShapeToHostShape(buffers_.shape()));
  }
  // TODO(b/170310047): remove this overload.
  ExecutionInput(ShapeTree<MaybeOwningDeviceMemory> buffers,
                 xla::Shape host_shape)
      : buffers_(std::move(buffers)) {
    SetHostShape(ShapeUtil::DeviceShapeToHostShape(buffers_.shape()));
  }

  ExecutionInput(ExecutionInput&&) = default;

  ~ExecutionInput();

  ExecutionInput& operator=(ExecutionInput&&) = default;

  const Shape& shape() const {
    return dynamic_shape_ != nullptr ? *dynamic_shape_ : buffers_.shape();
  }

  const Shape& host_shape() const {
    return host_shape_ != nullptr ? *host_shape_ : shape();
  }

  Status SetDynamicShape(Shape dynamic_shape);

  xla::StatusOr<xla::ShapedBuffer> ToShapedBuffer(
      se::DeviceMemoryAllocator* allocator, int device_ordinal) const;

  void SetBuffer(const ShapeIndex& index, MaybeOwningDeviceMemory buffer) {
    *buffers_.mutable_element(index) = std::move(buffer);
  }

  void SetUnownedBuffer(const ShapeIndex& index,
                        MaybeOwningDeviceMemory buffer);

  void SetUnownedIndex(const ShapeIndex& index) {
    unowned_indices_.insert(index);
  }

  void ClearUnownedIndex(const ShapeIndex& index) {
    unowned_indices_.erase(index);
  }

  const std::set<ShapeIndex>& unowned_indices() { return unowned_indices_; }

  const ShapeTree<MaybeOwningDeviceMemory>& Buffers() const { return buffers_; }

  ShapeTree<MaybeOwningDeviceMemory>* MutableBuffers() { return &buffers_; }

  MaybeOwningDeviceMemory* MutableBuffer(const ShapeIndex& index) {
    return buffers_.mutable_element(index);
  }

  const MaybeOwningDeviceMemory& Buffer(const ShapeIndex& index) const {
    return buffers_.element(index);
  }

 private:
  void SetHostShape(xla::Shape host_shape) {
    if (shape() != host_shape) {
      host_shape_ = absl::make_unique<Shape>(std::move(host_shape));
    }
  }

  ShapeTree<MaybeOwningDeviceMemory> buffers_;
  // Set of indices of buffers that should be returned to the caller if an error
  // occurs when enqueuing the computation.
  std::set<ShapeIndex> unowned_indices_;
  std::unique_ptr<Shape> dynamic_shape_;
  std::unique_ptr<Shape> host_shape_;
};

// ExecutionOutput encapsulates the output buffers of a execution and the
// leftover buffers to be released by the caller.
class ExecutionOutput {
 public:
  explicit ExecutionOutput(ScopedShapedBuffer result)
      : result_(std::move(result)) {}
  ExecutionOutput(ScopedShapedBuffer result,
                  std::vector<se::OwningDeviceMemory> to_be_released)
      : result_(std::move(result)),
        to_be_released_(std::move(to_be_released)) {}
  // TODO(b/170310047): remove this overload.
  ExecutionOutput(Shape on_host_shape, Shape on_device_shape,
                  se::DeviceMemoryAllocator* allocator, int device_ordinal)
      : result_(std::move(on_device_shape), allocator, device_ordinal) {}
  ExecutionOutput(Shape on_device_shape, se::DeviceMemoryAllocator* allocator,
                  int device_ordinal)
      : result_(std::move(on_device_shape), allocator, device_ordinal) {}
  ExecutionOutput(ExecutionOutput&&) = default;
  ExecutionOutput& operator=(ExecutionOutput&&) = default;

  ~ExecutionOutput() {
    // If the ExecutionOutput has not been committed, and if there are aliased
    // indices, clear them off the ScopedShapedBuffer to prevent them to be
    // released.
    for (auto& index : aliased_indices_) {
      result_.set_buffer(se::OwningDeviceMemory(), index);
    }
  }

  void AddAliasedIndex(ShapeIndex index) {
    aliased_indices_.push_back(std::move(index));
  }

  void AddToBeReleased(se::OwningDeviceMemory mem) {
    to_be_released_.push_back(std::move(mem));
  }

  // Should be called once it is known that the execute operation succeeded,
  // before returning the ExecutionOutput to the caller.
  ExecutionOutput& Commit() {
    aliased_indices_.clear();
    return *this;
  }

  const ScopedShapedBuffer& Result() const { return result_; }

  ScopedShapedBuffer* MutableResult() { return &result_; }

  ScopedShapedBuffer ConsumeResult() {
    aliased_indices_.clear();
    return std::move(result_);
  }

  const std::vector<se::OwningDeviceMemory>& ToBeReleased() const {
    return to_be_released_;
  }

  std::vector<se::OwningDeviceMemory> ConsumeToBeReleased() {
    return std::move(to_be_released_);
  }

  std::vector<ShapeIndex> ConsumeAliasedIndices() {
    auto aliased = std::move(aliased_indices_);
    aliased_indices_.clear();
    return aliased;
  }

 private:
  ScopedShapedBuffer result_;

  // Leftover buffers for the caller to release. Elements in this list are
  // donated input memory buffers that are not reused by XLA as outputs.
  std::vector<se::OwningDeviceMemory> to_be_released_;

  // These are the indices in result_ which have been aliased from the caller.
  // If the execution operation fails, the caller should maintain ownership of
  // the buffer, so we track the indices here, and unless the ExecutionOutput is
  // committed, we remove them from the result_ before destruction.
  std::vector<ShapeIndex> aliased_indices_;

  // A shape table is a continuous region in memory that is used to hold the
  // runtime dimension sizes of dynamic output shapes.
  se::OwningDeviceMemory output_shape_table_;
};

// A given platform's compiler will produce an Executable -- this is a uniform
// interface that is used for launching compiled programs across platforms.
class Executable {
 public:
  explicit Executable(std::shared_ptr<HloModule> hlo_module)
      : hlo_module_(std::move(hlo_module)) {}

  // TODO(b/172012028): Remove this constructor.
  explicit Executable(
      std::shared_ptr<HloModule> hlo_module,
      std::unique_ptr<HloProfilePrinterData> hlo_profile_printer_data,
      std::unique_ptr<HloProfileIndexMap> hlo_profile_index_map)
      : hlo_module_(std::move(hlo_module)),
        hlo_profile_printer_data_(std::move(hlo_profile_printer_data)),
        hlo_profile_index_map_(std::move(hlo_profile_index_map)) {
    CHECK_EQ(hlo_profile_printer_data_.get() == nullptr,
             hlo_profile_index_map_.get() == nullptr);
  }
  virtual ~Executable() {}

  // Enqueues the compilation result on the provided stream, passing the given
  // arguments. This call is blocking and returns after the execution is done.
  //
  // If the hlo_execution_profile is provided as non-nullptr, profiling will be
  // enabled.
  //
  // Returns a shaped buffer containing the result of the computation.
  StatusOr<ScopedShapedBuffer> ExecuteOnStream(
      const ServiceExecutableRunOptions* run_options,
      absl::Span<const ShapedBuffer* const> arguments,
      HloExecutionProfile* hlo_execution_profile);

  // Starts the given program executing on the given stream/executor.
  //
  // `arguments` are ShapeTree containing the input parameters. For each element
  // in the shape tree, if the element holds the ownership of the memory, it is
  // considered donated and XLA will potentially reuse it as output buffers. For
  // all donated inputs, XLA is also responsible for freeing them.
  //
  // If an input is donated to XLA but is not reused as output, it is returned
  // as an leftover buffer for the caller to release.
  //
  // This call should be non-blocking and may return as soon as all of the
  // operations are enqueued for launch on the stream. Note that some
  // implementations may in fact block or may block in some circumstances (e.g.,
  // when profiling); i.e., asynchronous is a "may" not a "must".
  //
  // If the hlo_execution_profile is provided as non-nullptr, profiling will be
  // enabled. Note that profiling is tricky to use correctly, as the profiling
  // objects (when they exist) must out-live the task.
  virtual StatusOr<ScopedShapedBuffer> ExecuteAsyncOnStream(
      const ServiceExecutableRunOptions* run_options,
      absl::Span<const ShapedBuffer* const> arguments,
      HloExecutionProfile* hlo_execution_profile);

  // Same as ExecuteAsyncOnStream(), but blocks waiting for the computation to
  // complete.
  StatusOr<ExecutionOutput> ExecuteOnStream(
      const ServiceExecutableRunOptions* run_options,
      std::vector<ExecutionInput> arguments,
      HloExecutionProfile* hlo_execution_profile);

  virtual StatusOr<ExecutionOutput> ExecuteAsyncOnStream(
      const ServiceExecutableRunOptions* run_options,
      std::vector<ExecutionInput> arguments,
      HloExecutionProfile* hlo_execution_profile) = 0;

  // Same as ExecuteOnStream(), but runs this executable on multiple
  // streams. arguments[i] contains the arguments to the execution on
  // run_options[i]->stream() and the returned value is at index i of the
  // returned vector.
  virtual StatusOr<std::vector<ScopedShapedBuffer>> ExecuteOnStreams(
      absl::Span<const ServiceExecutableRunOptions> run_options,
      absl::Span<const absl::Span<const ShapedBuffer* const>> arguments);

  // Populates `hlo_execution_profile` from `executor`. This is implicit in any
  // Execute* API call that takes a hlo_execution_profile argument, but must be
  // called explicitly for other (async, for example) variants after the stream
  // has completed.
  virtual Status PopulateExecutionProfile(
      ExecutionProfile* execution_profile,
      HloExecutionProfile* hlo_execution_profile, se::Stream* stream) {
    return OkStatus();
  }

  // Convenience wrapper for calling Executable::ExecuteOnStream. Sets up a
  // timer for the execution, sets up HLO profiling if enabled, and fills in the
  // given ExecutionProfile if non-null.
  StatusOr<ScopedShapedBuffer> ExecuteOnStreamWrapper(
      const ServiceExecutableRunOptions* run_options,
      absl::Span<const ShapedBuffer* const> arguments);

  StatusOr<ExecutionOutput> ExecuteOnStreamWrapper(
      const ServiceExecutableRunOptions* run_options,
      std::vector<ExecutionInput> arguments);

  StatusOr<ScopedShapedBuffer> ExecuteAsyncOnStreamWrapper(
      const ServiceExecutableRunOptions* run_options,
      absl::Span<const ShapedBuffer* const> arguments);

  StatusOr<ExecutionOutput> ExecuteAsyncOnStreamWrapper(
      const ServiceExecutableRunOptions* run_options,
      std::vector<ExecutionInput> arguments);

  const HloProfilePrinterData& hlo_profile_printer_data() const {
    CHECK(hlo_profiling_enabled());
    return *hlo_profile_printer_data_;
  }

  const HloProfileIndexMap& hlo_profile_index_map() const {
    CHECK(hlo_profiling_enabled());
    return *hlo_profile_index_map_;
  }

  // Returns whether this executable was compiled with HLO profilings support
  // enabled. If not, the caller should not expect an hlo_execution_profile
  // passed to ExecuteOnStream above to be populated during execution.
  bool hlo_profiling_enabled() const {
    return hlo_profile_printer_data_ != nullptr;
  }

  HloModule& module() const { return *hlo_module_; }
  std::shared_ptr<HloModule> shared_module() const { return hlo_module_; }

  const bool has_module() const { return hlo_module_ != nullptr; }

  const HloModuleConfig& module_config() const { return hlo_module_->config(); }

  // The shape (including layout) that results from this execution. This is the
  // shape of the DeviceMemoryBase result value in ExecuteOnStream above.
  const Shape& result_shape() const {
    return hlo_module_->config().entry_computation_layout().result_shape();
  }

  // Returns the size of the executable in bytes. Returns -1 if this query is
  // not supported by the executable.
  //
  // Does not include the size of used libraries (e.g. cuDNN, Eigen, etc.).
  virtual int64_t SizeOfGeneratedCodeInBytes() const;

  // Dumping helpers.
  void set_hlo_proto(std::unique_ptr<xla::HloProto> hlo_proto) {
    hlo_proto_ = std::move(hlo_proto);
  }
  bool dumping_snapshot() const { return hlo_proto_ != nullptr; }
  HloProto const* hlo_proto() const { return hlo_proto_.get(); }

  std::string& debug_info() { return debug_info_; }
  void set_debug_info(const std::string& debug_info) {
    debug_info_ = debug_info;
  }
  // Gather unused but donated buffers, return them to the caller of this API.
  // We don't free buffers inside this function since the caller could have
  // different preferences for buffer deallocation. For example, in TensorFlow,
  // buffers are mostly efficiently deallocated as soon as a program has been
  // launched. However, in XRT, the buffers are expected to be deallocated after
  // the program has finished since XRT doesn't support async deallocation.
  void MarkToBeReleasedArguments(absl::Span<ExecutionInput> arguments,
                                 ExecutionOutput& result);

 protected:
  // HloModule this was compiled from. BufferAssignment keeps pointers to
  // HloInstructions owned by the HloModule so we need to keep the HloModule
  // around.
  const std::shared_ptr<HloModule> hlo_module_;

  // The serialized HLO proto. Non-null only if dumping snapshots is enabled.
  std::unique_ptr<HloProto const> hlo_proto_;

  // Execution count, used to generate a unique filename for each dumped
  // execution.
  int64_t execution_count_ = 0;

  std::unique_ptr<HloProfilePrinterData> hlo_profile_printer_data_;
  std::unique_ptr<HloProfileIndexMap> hlo_profile_index_map_;

  // Generic debug information as a string.
  std::string debug_info_;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_EXECUTABLE_H_
