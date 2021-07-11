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
// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_SERIALIZATION_TFLITE_GPU_CL_DATA_H_
#define FLATBUFFERS_GENERATED_SERIALIZATION_TFLITE_GPU_CL_DATA_H_

#include "flatbuffers/flatbuffers.h"

#include "tensorflow/lite/delegates/gpu/common/task/serialization_base_generated.h"

namespace tflite {
namespace gpu {
namespace cl {
namespace data {

struct TensorDescWithId;
struct TensorDescWithIdBuilder;

struct CLNode;
struct CLNodeBuilder;

struct PairOfValueIds;
struct PairOfValueIdsBuilder;

struct BinaryProgram;
struct BinaryProgramBuilder;

struct InferenceContext;
struct InferenceContextBuilder;

struct TensorDescWithId FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef TensorDescWithIdBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_DESC = 4,
    VT_ID = 6
  };
  const tflite::gpu::data::TensorDescriptor *desc() const {
    return GetPointer<const tflite::gpu::data::TensorDescriptor *>(VT_DESC);
  }
  int32_t id() const {
    return GetField<int32_t>(VT_ID, 0);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_DESC) &&
           verifier.VerifyTable(desc()) &&
           VerifyField<int32_t>(verifier, VT_ID) &&
           verifier.EndTable();
  }
};

struct TensorDescWithIdBuilder {
  typedef TensorDescWithId Table;
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_desc(flatbuffers::Offset<tflite::gpu::data::TensorDescriptor> desc) {
    fbb_.AddOffset(TensorDescWithId::VT_DESC, desc);
  }
  void add_id(int32_t id) {
    fbb_.AddElement<int32_t>(TensorDescWithId::VT_ID, id, 0);
  }
  explicit TensorDescWithIdBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  flatbuffers::Offset<TensorDescWithId> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<TensorDescWithId>(end);
    return o;
  }
};

inline flatbuffers::Offset<TensorDescWithId> CreateTensorDescWithId(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<tflite::gpu::data::TensorDescriptor> desc = 0,
    int32_t id = 0) {
  TensorDescWithIdBuilder builder_(_fbb);
  builder_.add_id(id);
  builder_.add_desc(desc);
  return builder_.Finish();
}

struct CLNode FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef CLNodeBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_GPU_OP = 4,
    VT_FINGERPRINT = 6,
    VT_INPUT_IDS = 8,
    VT_OUTPUT_IDS = 10,
    VT_NAME = 12
  };
  const tflite::gpu::data::GPUOperation *gpu_op() const {
    return GetPointer<const tflite::gpu::data::GPUOperation *>(VT_GPU_OP);
  }
  uint64_t fingerprint() const { return GetField<uint64_t>(VT_FINGERPRINT, 0); }
  const flatbuffers::Vector<int32_t> *input_ids() const {
    return GetPointer<const flatbuffers::Vector<int32_t> *>(VT_INPUT_IDS);
  }
  const flatbuffers::Vector<int32_t> *output_ids() const {
    return GetPointer<const flatbuffers::Vector<int32_t> *>(VT_OUTPUT_IDS);
  }
  const flatbuffers::String *name() const {
    return GetPointer<const flatbuffers::String *>(VT_NAME);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) && VerifyOffset(verifier, VT_GPU_OP) &&
           verifier.VerifyTable(gpu_op()) &&
           VerifyField<uint64_t>(verifier, VT_FINGERPRINT) &&
           VerifyOffset(verifier, VT_INPUT_IDS) &&
           verifier.VerifyVector(input_ids()) &&
           VerifyOffset(verifier, VT_OUTPUT_IDS) &&
           verifier.VerifyVector(output_ids()) &&
           VerifyOffset(verifier, VT_NAME) && verifier.VerifyString(name()) &&
           verifier.EndTable();
  }
};

struct CLNodeBuilder {
  typedef CLNode Table;
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_gpu_op(flatbuffers::Offset<tflite::gpu::data::GPUOperation> gpu_op) {
    fbb_.AddOffset(CLNode::VT_GPU_OP, gpu_op);
  }
  void add_fingerprint(uint64_t fingerprint) {
    fbb_.AddElement<uint64_t>(CLNode::VT_FINGERPRINT, fingerprint, 0);
  }
  void add_input_ids(flatbuffers::Offset<flatbuffers::Vector<int32_t>> input_ids) {
    fbb_.AddOffset(CLNode::VT_INPUT_IDS, input_ids);
  }
  void add_output_ids(flatbuffers::Offset<flatbuffers::Vector<int32_t>> output_ids) {
    fbb_.AddOffset(CLNode::VT_OUTPUT_IDS, output_ids);
  }
  void add_name(flatbuffers::Offset<flatbuffers::String> name) {
    fbb_.AddOffset(CLNode::VT_NAME, name);
  }
  explicit CLNodeBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  flatbuffers::Offset<CLNode> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<CLNode>(end);
    return o;
  }
};

inline flatbuffers::Offset<CLNode> CreateCLNode(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<tflite::gpu::data::GPUOperation> gpu_op = 0,
    uint64_t fingerprint = 0,
    flatbuffers::Offset<flatbuffers::Vector<int32_t>> input_ids = 0,
    flatbuffers::Offset<flatbuffers::Vector<int32_t>> output_ids = 0,
    flatbuffers::Offset<flatbuffers::String> name = 0) {
  CLNodeBuilder builder_(_fbb);
  builder_.add_fingerprint(fingerprint);
  builder_.add_name(name);
  builder_.add_output_ids(output_ids);
  builder_.add_input_ids(input_ids);
  builder_.add_gpu_op(gpu_op);
  return builder_.Finish();
}

inline flatbuffers::Offset<CLNode> CreateCLNodeDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<tflite::gpu::data::GPUOperation> gpu_op = 0,
    uint64_t fingerprint = 0, const std::vector<int32_t> *input_ids = nullptr,
    const std::vector<int32_t> *output_ids = nullptr,
    const char *name = nullptr) {
  auto input_ids__ = input_ids ? _fbb.CreateVector<int32_t>(*input_ids) : 0;
  auto output_ids__ = output_ids ? _fbb.CreateVector<int32_t>(*output_ids) : 0;
  auto name__ = name ? _fbb.CreateString(name) : 0;
  return tflite::gpu::cl::data::CreateCLNode(_fbb, gpu_op, fingerprint,
                                             input_ids__, output_ids__, name__);
}

struct PairOfValueIds FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef PairOfValueIdsBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_FIRST = 4,
    VT_SECOND = 6
  };
  int32_t first() const {
    return GetField<int32_t>(VT_FIRST, 0);
  }
  int32_t second() const {
    return GetField<int32_t>(VT_SECOND, 0);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<int32_t>(verifier, VT_FIRST) &&
           VerifyField<int32_t>(verifier, VT_SECOND) &&
           verifier.EndTable();
  }
};

struct PairOfValueIdsBuilder {
  typedef PairOfValueIds Table;
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_first(int32_t first) {
    fbb_.AddElement<int32_t>(PairOfValueIds::VT_FIRST, first, 0);
  }
  void add_second(int32_t second) {
    fbb_.AddElement<int32_t>(PairOfValueIds::VT_SECOND, second, 0);
  }
  explicit PairOfValueIdsBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  flatbuffers::Offset<PairOfValueIds> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<PairOfValueIds>(end);
    return o;
  }
};

inline flatbuffers::Offset<PairOfValueIds> CreatePairOfValueIds(
    flatbuffers::FlatBufferBuilder &_fbb,
    int32_t first = 0,
    int32_t second = 0) {
  PairOfValueIdsBuilder builder_(_fbb);
  builder_.add_second(second);
  builder_.add_first(first);
  return builder_.Finish();
}

struct BinaryProgram FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef BinaryProgramBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_FINGERPRINT = 4,
    VT_BINARY = 6
  };
  uint64_t fingerprint() const { return GetField<uint64_t>(VT_FINGERPRINT, 0); }
  const flatbuffers::Vector<uint8_t> *binary() const {
    return GetPointer<const flatbuffers::Vector<uint8_t> *>(VT_BINARY);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<uint64_t>(verifier, VT_FINGERPRINT) &&
           VerifyOffset(verifier, VT_BINARY) &&
           verifier.VerifyVector(binary()) && verifier.EndTable();
  }
};

struct BinaryProgramBuilder {
  typedef BinaryProgram Table;
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_fingerprint(uint64_t fingerprint) {
    fbb_.AddElement<uint64_t>(BinaryProgram::VT_FINGERPRINT, fingerprint, 0);
  }
  void add_binary(flatbuffers::Offset<flatbuffers::Vector<uint8_t>> binary) {
    fbb_.AddOffset(BinaryProgram::VT_BINARY, binary);
  }
  explicit BinaryProgramBuilder(flatbuffers::FlatBufferBuilder &_fbb)
      : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  flatbuffers::Offset<BinaryProgram> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<BinaryProgram>(end);
    return o;
  }
};

inline flatbuffers::Offset<BinaryProgram> CreateBinaryProgram(
    flatbuffers::FlatBufferBuilder &_fbb, uint64_t fingerprint = 0,
    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> binary = 0) {
  BinaryProgramBuilder builder_(_fbb);
  builder_.add_fingerprint(fingerprint);
  builder_.add_binary(binary);
  return builder_.Finish();
}

inline flatbuffers::Offset<BinaryProgram> CreateBinaryProgramDirect(
    flatbuffers::FlatBufferBuilder &_fbb, uint64_t fingerprint = 0,
    const std::vector<uint8_t> *binary = nullptr) {
  auto binary__ = binary ? _fbb.CreateVector<uint8_t>(*binary) : 0;
  return tflite::gpu::cl::data::CreateBinaryProgram(_fbb, fingerprint,
                                                    binary__);
}

struct InferenceContext FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef InferenceContextBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_DRIVER_VERSION = 4,
    VT_BINARY_PROGRAMS = 6,
    VT_NEED_FLUSH = 8,
    VT_FLUSH_PERIODICALLY = 10,
    VT_FLUSH_PERIOD = 12,
    VT_NEED_MANUAL_RELEASE = 14,
    VT_PRECISION = 16,
    VT_STORAGE_TYPE = 18,
    VT_NODES = 20,
    VT_TENSORS = 22,
    VT_CONST_TENSORS = 24,
    VT_INPUT_IDS = 26,
    VT_VARIABLE_IDS_AND_REFS = 28,
    VT_OUTPUT_IDS = 30,
    VT_INPUT_REFS = 32,
    VT_OUTPUT_REFS = 34
  };
  const flatbuffers::String *driver_version() const {
    return GetPointer<const flatbuffers::String *>(VT_DRIVER_VERSION);
  }
  const flatbuffers::Vector<
      flatbuffers::Offset<tflite::gpu::cl::data::BinaryProgram>>
      *binary_programs() const {
    return GetPointer<const flatbuffers::Vector<
        flatbuffers::Offset<tflite::gpu::cl::data::BinaryProgram>> *>(
        VT_BINARY_PROGRAMS);
  }
  bool need_flush() const {
    return GetField<uint8_t>(VT_NEED_FLUSH, 0) != 0;
  }
  bool flush_periodically() const {
    return GetField<uint8_t>(VT_FLUSH_PERIODICALLY, 0) != 0;
  }
  int32_t flush_period() const {
    return GetField<int32_t>(VT_FLUSH_PERIOD, 0);
  }
  bool need_manual_release() const {
    return GetField<uint8_t>(VT_NEED_MANUAL_RELEASE, 0) != 0;
  }
  tflite::gpu::data::CalculationsPrecision precision() const {
    return static_cast<tflite::gpu::data::CalculationsPrecision>(
        GetField<int8_t>(VT_PRECISION, 0));
  }
  tflite::gpu::data::TensorStorageType storage_type() const {
    return static_cast<tflite::gpu::data::TensorStorageType>(GetField<int8_t>(VT_STORAGE_TYPE, 0));
  }
  const flatbuffers::Vector<flatbuffers::Offset<tflite::gpu::cl::data::CLNode>> *nodes() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<tflite::gpu::cl::data::CLNode>> *>(VT_NODES);
  }
  const flatbuffers::Vector<flatbuffers::Offset<tflite::gpu::cl::data::TensorDescWithId>> *tensors() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<tflite::gpu::cl::data::TensorDescWithId>> *>(VT_TENSORS);
  }
  const flatbuffers::Vector<
      flatbuffers::Offset<tflite::gpu::cl::data::TensorDescWithId>>
      *const_tensors() const {
    return GetPointer<const flatbuffers::Vector<
        flatbuffers::Offset<tflite::gpu::cl::data::TensorDescWithId>> *>(
        VT_CONST_TENSORS);
  }
  const flatbuffers::Vector<int32_t> *input_ids() const {
    return GetPointer<const flatbuffers::Vector<int32_t> *>(VT_INPUT_IDS);
  }
  const flatbuffers::Vector<flatbuffers::Offset<tflite::gpu::cl::data::PairOfValueIds>> *variable_ids_and_refs() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<tflite::gpu::cl::data::PairOfValueIds>> *>(VT_VARIABLE_IDS_AND_REFS);
  }
  const flatbuffers::Vector<int32_t> *output_ids() const {
    return GetPointer<const flatbuffers::Vector<int32_t> *>(VT_OUTPUT_IDS);
  }
  const flatbuffers::Vector<int64_t> *input_refs() const {
    return GetPointer<const flatbuffers::Vector<int64_t> *>(VT_INPUT_REFS);
  }
  const flatbuffers::Vector<int64_t> *output_refs() const {
    return GetPointer<const flatbuffers::Vector<int64_t> *>(VT_OUTPUT_REFS);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_DRIVER_VERSION) &&
           verifier.VerifyString(driver_version()) &&
           VerifyOffset(verifier, VT_BINARY_PROGRAMS) &&
           verifier.VerifyVector(binary_programs()) &&
           verifier.VerifyVectorOfTables(binary_programs()) &&
           VerifyField<uint8_t>(verifier, VT_NEED_FLUSH) &&
           VerifyField<uint8_t>(verifier, VT_FLUSH_PERIODICALLY) &&
           VerifyField<int32_t>(verifier, VT_FLUSH_PERIOD) &&
           VerifyField<uint8_t>(verifier, VT_NEED_MANUAL_RELEASE) &&
           VerifyField<int8_t>(verifier, VT_PRECISION) &&
           VerifyField<int8_t>(verifier, VT_STORAGE_TYPE) &&
           VerifyOffset(verifier, VT_NODES) && verifier.VerifyVector(nodes()) &&
           verifier.VerifyVectorOfTables(nodes()) &&
           VerifyOffset(verifier, VT_TENSORS) &&
           verifier.VerifyVector(tensors()) &&
           verifier.VerifyVectorOfTables(tensors()) &&
           VerifyOffset(verifier, VT_CONST_TENSORS) &&
           verifier.VerifyVector(const_tensors()) &&
           verifier.VerifyVectorOfTables(const_tensors()) &&
           VerifyOffset(verifier, VT_INPUT_IDS) &&
           verifier.VerifyVector(input_ids()) &&
           VerifyOffset(verifier, VT_VARIABLE_IDS_AND_REFS) &&
           verifier.VerifyVector(variable_ids_and_refs()) &&
           verifier.VerifyVectorOfTables(variable_ids_and_refs()) &&
           VerifyOffset(verifier, VT_OUTPUT_IDS) &&
           verifier.VerifyVector(output_ids()) &&
           VerifyOffset(verifier, VT_INPUT_REFS) &&
           verifier.VerifyVector(input_refs()) &&
           VerifyOffset(verifier, VT_OUTPUT_REFS) &&
           verifier.VerifyVector(output_refs()) && verifier.EndTable();
  }
};

struct InferenceContextBuilder {
  typedef InferenceContext Table;
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_driver_version(
      flatbuffers::Offset<flatbuffers::String> driver_version) {
    fbb_.AddOffset(InferenceContext::VT_DRIVER_VERSION, driver_version);
  }
  void add_binary_programs(
      flatbuffers::Offset<flatbuffers::Vector<
          flatbuffers::Offset<tflite::gpu::cl::data::BinaryProgram>>>
          binary_programs) {
    fbb_.AddOffset(InferenceContext::VT_BINARY_PROGRAMS, binary_programs);
  }
  void add_need_flush(bool need_flush) {
    fbb_.AddElement<uint8_t>(InferenceContext::VT_NEED_FLUSH, static_cast<uint8_t>(need_flush), 0);
  }
  void add_flush_periodically(bool flush_periodically) {
    fbb_.AddElement<uint8_t>(InferenceContext::VT_FLUSH_PERIODICALLY, static_cast<uint8_t>(flush_periodically), 0);
  }
  void add_flush_period(int32_t flush_period) {
    fbb_.AddElement<int32_t>(InferenceContext::VT_FLUSH_PERIOD, flush_period, 0);
  }
  void add_need_manual_release(bool need_manual_release) {
    fbb_.AddElement<uint8_t>(InferenceContext::VT_NEED_MANUAL_RELEASE, static_cast<uint8_t>(need_manual_release), 0);
  }
  void add_precision(tflite::gpu::data::CalculationsPrecision precision) {
    fbb_.AddElement<int8_t>(InferenceContext::VT_PRECISION, static_cast<int8_t>(precision), 0);
  }
  void add_storage_type(tflite::gpu::data::TensorStorageType storage_type) {
    fbb_.AddElement<int8_t>(InferenceContext::VT_STORAGE_TYPE, static_cast<int8_t>(storage_type), 0);
  }
  void add_nodes(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<tflite::gpu::cl::data::CLNode>>> nodes) {
    fbb_.AddOffset(InferenceContext::VT_NODES, nodes);
  }
  void add_tensors(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<tflite::gpu::cl::data::TensorDescWithId>>> tensors) {
    fbb_.AddOffset(InferenceContext::VT_TENSORS, tensors);
  }
  void add_const_tensors(
      flatbuffers::Offset<flatbuffers::Vector<
          flatbuffers::Offset<tflite::gpu::cl::data::TensorDescWithId>>>
          const_tensors) {
    fbb_.AddOffset(InferenceContext::VT_CONST_TENSORS, const_tensors);
  }
  void add_input_ids(flatbuffers::Offset<flatbuffers::Vector<int32_t>> input_ids) {
    fbb_.AddOffset(InferenceContext::VT_INPUT_IDS, input_ids);
  }
  void add_variable_ids_and_refs(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<tflite::gpu::cl::data::PairOfValueIds>>> variable_ids_and_refs) {
    fbb_.AddOffset(InferenceContext::VT_VARIABLE_IDS_AND_REFS, variable_ids_and_refs);
  }
  void add_output_ids(flatbuffers::Offset<flatbuffers::Vector<int32_t>> output_ids) {
    fbb_.AddOffset(InferenceContext::VT_OUTPUT_IDS, output_ids);
  }
  void add_input_refs(flatbuffers::Offset<flatbuffers::Vector<int64_t>> input_refs) {
    fbb_.AddOffset(InferenceContext::VT_INPUT_REFS, input_refs);
  }
  void add_output_refs(flatbuffers::Offset<flatbuffers::Vector<int64_t>> output_refs) {
    fbb_.AddOffset(InferenceContext::VT_OUTPUT_REFS, output_refs);
  }
  explicit InferenceContextBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  flatbuffers::Offset<InferenceContext> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<InferenceContext>(end);
    return o;
  }
};

inline flatbuffers::Offset<InferenceContext> CreateInferenceContext(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::String> driver_version = 0,
    flatbuffers::Offset<flatbuffers::Vector<
        flatbuffers::Offset<tflite::gpu::cl::data::BinaryProgram>>>
        binary_programs = 0,
    bool need_flush = false, bool flush_periodically = false,
    int32_t flush_period = 0, bool need_manual_release = false,
    tflite::gpu::data::CalculationsPrecision precision =
        tflite::gpu::data::CalculationsPrecision::F32,
    tflite::gpu::data::TensorStorageType storage_type =
        tflite::gpu::data::TensorStorageType::UNKNOWN,
    flatbuffers::Offset<
        flatbuffers::Vector<flatbuffers::Offset<tflite::gpu::cl::data::CLNode>>>
        nodes = 0,
    flatbuffers::Offset<flatbuffers::Vector<
        flatbuffers::Offset<tflite::gpu::cl::data::TensorDescWithId>>>
        tensors = 0,
    flatbuffers::Offset<flatbuffers::Vector<
        flatbuffers::Offset<tflite::gpu::cl::data::TensorDescWithId>>>
        const_tensors = 0,
    flatbuffers::Offset<flatbuffers::Vector<int32_t>> input_ids = 0,
    flatbuffers::Offset<flatbuffers::Vector<
        flatbuffers::Offset<tflite::gpu::cl::data::PairOfValueIds>>>
        variable_ids_and_refs = 0,
    flatbuffers::Offset<flatbuffers::Vector<int32_t>> output_ids = 0,
    flatbuffers::Offset<flatbuffers::Vector<int64_t>> input_refs = 0,
    flatbuffers::Offset<flatbuffers::Vector<int64_t>> output_refs = 0) {
  InferenceContextBuilder builder_(_fbb);
  builder_.add_output_refs(output_refs);
  builder_.add_input_refs(input_refs);
  builder_.add_output_ids(output_ids);
  builder_.add_variable_ids_and_refs(variable_ids_and_refs);
  builder_.add_input_ids(input_ids);
  builder_.add_const_tensors(const_tensors);
  builder_.add_tensors(tensors);
  builder_.add_nodes(nodes);
  builder_.add_flush_period(flush_period);
  builder_.add_binary_programs(binary_programs);
  builder_.add_driver_version(driver_version);
  builder_.add_storage_type(storage_type);
  builder_.add_precision(precision);
  builder_.add_need_manual_release(need_manual_release);
  builder_.add_flush_periodically(flush_periodically);
  builder_.add_need_flush(need_flush);
  return builder_.Finish();
}

inline flatbuffers::Offset<InferenceContext> CreateInferenceContextDirect(
    flatbuffers::FlatBufferBuilder &_fbb, const char *driver_version = nullptr,
    const std::vector<flatbuffers::Offset<tflite::gpu::cl::data::BinaryProgram>>
        *binary_programs = nullptr,
    bool need_flush = false, bool flush_periodically = false,
    int32_t flush_period = 0, bool need_manual_release = false,
    tflite::gpu::data::CalculationsPrecision precision =
        tflite::gpu::data::CalculationsPrecision::F32,
    tflite::gpu::data::TensorStorageType storage_type =
        tflite::gpu::data::TensorStorageType::UNKNOWN,
    const std::vector<flatbuffers::Offset<tflite::gpu::cl::data::CLNode>>
        *nodes = nullptr,
    const std::vector<
        flatbuffers::Offset<tflite::gpu::cl::data::TensorDescWithId>> *tensors =
        nullptr,
    const std::vector<flatbuffers::Offset<
        tflite::gpu::cl::data::TensorDescWithId>> *const_tensors = nullptr,
    const std::vector<int32_t> *input_ids = nullptr,
    const std::vector<
        flatbuffers::Offset<tflite::gpu::cl::data::PairOfValueIds>>
        *variable_ids_and_refs = nullptr,
    const std::vector<int32_t> *output_ids = nullptr,
    const std::vector<int64_t> *input_refs = nullptr,
    const std::vector<int64_t> *output_refs = nullptr) {
  auto driver_version__ =
      driver_version ? _fbb.CreateString(driver_version) : 0;
  auto binary_programs__ =
      binary_programs
          ? _fbb.CreateVector<
                flatbuffers::Offset<tflite::gpu::cl::data::BinaryProgram>>(
                *binary_programs)
          : 0;
  auto nodes__ = nodes ? _fbb.CreateVector<flatbuffers::Offset<tflite::gpu::cl::data::CLNode>>(*nodes) : 0;
  auto tensors__ = tensors ? _fbb.CreateVector<flatbuffers::Offset<tflite::gpu::cl::data::TensorDescWithId>>(*tensors) : 0;
  auto const_tensors__ =
      const_tensors
          ? _fbb.CreateVector<
                flatbuffers::Offset<tflite::gpu::cl::data::TensorDescWithId>>(
                *const_tensors)
          : 0;
  auto input_ids__ = input_ids ? _fbb.CreateVector<int32_t>(*input_ids) : 0;
  auto variable_ids_and_refs__ = variable_ids_and_refs ? _fbb.CreateVector<flatbuffers::Offset<tflite::gpu::cl::data::PairOfValueIds>>(*variable_ids_and_refs) : 0;
  auto output_ids__ = output_ids ? _fbb.CreateVector<int32_t>(*output_ids) : 0;
  auto input_refs__ = input_refs ? _fbb.CreateVector<int64_t>(*input_refs) : 0;
  auto output_refs__ = output_refs ? _fbb.CreateVector<int64_t>(*output_refs) : 0;
  return tflite::gpu::cl::data::CreateInferenceContext(
      _fbb, driver_version__, binary_programs__, need_flush, flush_periodically,
      flush_period, need_manual_release, precision, storage_type, nodes__,
      tensors__, const_tensors__, input_ids__, variable_ids_and_refs__,
      output_ids__, input_refs__, output_refs__);
}

inline const tflite::gpu::cl::data::InferenceContext *GetInferenceContext(const void *buf) {
  return flatbuffers::GetRoot<tflite::gpu::cl::data::InferenceContext>(buf);
}

inline const tflite::gpu::cl::data::InferenceContext *GetSizePrefixedInferenceContext(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<tflite::gpu::cl::data::InferenceContext>(buf);
}

inline bool VerifyInferenceContextBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<tflite::gpu::cl::data::InferenceContext>(nullptr);
}

inline bool VerifySizePrefixedInferenceContextBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<tflite::gpu::cl::data::InferenceContext>(nullptr);
}

inline void FinishInferenceContextBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<tflite::gpu::cl::data::InferenceContext> root) {
  fbb.Finish(root);
}

inline void FinishSizePrefixedInferenceContextBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<tflite::gpu::cl::data::InferenceContext> root) {
  fbb.FinishSizePrefixed(root);
}

}  // namespace data
}  // namespace cl
}  // namespace gpu
}  // namespace tflite

#endif  // FLATBUFFERS_GENERATED_SERIALIZATION_TFLITE_GPU_CL_DATA_H_
