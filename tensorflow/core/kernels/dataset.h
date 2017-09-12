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
#ifndef THIRD_PARTY_TENSORFLOW_CORE_KERNELS_DATASET_H_
#define THIRD_PARTY_TENSORFLOW_CORE_KERNELS_DATASET_H_

#include <memory>

#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/framework/resource_mgr.h"
#include "tensorflow/core/framework/variant_encode_decode.h"
#include "tensorflow/core/framework/variant_tensor_data.h"
#include "tensorflow/core/lib/strings/str_util.h"
#include "tensorflow/core/lib/strings/strcat.h"
#include "tensorflow/core/platform/tracing.h"
#include "tensorflow/core/util/tensor_bundle/naming.h"
#include "tensorflow/core/util/tensor_bundle/tensor_bundle.h"

// Polymorphic datasets should support all primitive TensorFlow
// types. Use this macro to expand `m(T)` once for each primitive type
// `T`, e.g. to build a `switch` statement.
#define TF_CALL_DATASET_TYPES(m) TF_CALL_ALL_TYPES(m) TF_CALL_QUANTIZED_TYPES(m)

namespace tensorflow {

class ResourceMgr;

// A cut-down version of OpKernelContext for running computations in
// iterators. Note that we cannot simply use OpKernelContext here
// because we might run computation in an iterator whose lifetime is
// not nested within the lifetime of a single OpKernelContext
// (e.g. asynchronous prefetching).
//
// TODO(mrry): We will probably need to support more of
// OpKernelContext here. For example, should allocation be handled by
// the IteratorContext?
// TODO(mrry): We will need to fabricate step IDs for calls to ops
// that are not nested within a particular step.
// TODO(mrry): We're making some daring assumptions about the lifetime
// of the FunctionLibraryRuntime and runner passed in here. Once
// created, a FunctionLibraryRuntime should stay alive for the
// remainder of a session, so we copy the pointer. A runner will be
// deleted when the original step ends, but all existing runners only
// close over session-lifetime (or longer-lived) state, so we can make
// a copy of the function. There's nothing in the definition of either
// class to guarantee that what we are doing is safe. We should
// formalize the properties here.
class IteratorContext {
 public:
  struct Params {
    // Interface to operating system functionality.
    Env* env;

    // The step being executed.
    int64 step_id = 0;

    // Shared resources accessible by this iterator invocation.
    ResourceMgr* resource_manager = nullptr;

    // Function call support.
    std::function<void(std::function<void()>)> runner = nullptr;
  };

  explicit IteratorContext(Params params) : params_(std::move(params)) {}

  Env* env() const { return params_.env; }

  int64 step_id() const { return params_.step_id; }

  std::function<void(std::function<void()>)>* runner() {
    return &params_.runner;
  }

  ResourceMgr* resource_manager() const { return params_.resource_manager; }

 private:
  Params params_;
};

// Represents the current position in a range of outputs, where the
// range of outputs is typically represented by an `DatasetBase`,
// defined below.
class IteratorBase {
 protected:
  class IteratorBundleReader;
  class IteratorBundleWriter;

 public:
  virtual ~IteratorBase() {}

  // Gets the next output from the range that this iterator is traversing.
  //
  // If at least one output remains in this iterator's range, that
  // output will be stored in `*out_tensors` and `false` will be
  // stored in `*end_of_sequence`.
  //
  // If no more outputs remain in this iterator's range, `true` will
  // be stored in `*end_of_sequence`, and the content of
  // `*out_tensors` will be undefined.
  //
  // This method is thread-safe.
  //
  // TODO(mrry): Define `GetNextAsync()` or `GetNextManyAsync()`, and
  // potentially remove this method.
  virtual Status GetNext(IteratorContext* ctx, std::vector<Tensor>* out_tensors,
                         bool* end_of_sequence) = 0;

  // Returns a vector of DataType values, representing the respective
  // element types of each tuple component in the outputs of this
  // iterator.
  virtual const DataTypeVector& output_dtypes() const = 0;

  // Returns a vector of tensor shapes, representing the respective
  // (and possibly partially defined) shapes of each tuple component
  // in the outputs of this iterator.
  virtual const std::vector<PartialTensorShape>& output_shapes() const = 0;

  // Saves the state of this iterator.
  virtual Status SaveState(OpKernelContext* ctx, StringPiece path) {
    BundleWriter bundle_writer(ctx->env(), path);
    IteratorBundleWriter writer(&bundle_writer);
    if (is_exhausted_) {
      LOG(INFO) << "Iterator exhausted. Nothing to save.";
      TF_RETURN_IF_ERROR(
          writer.WriteScalar<string>(kIteratorExhausted, kIteratorExhausted));
    } else {
      TF_RETURN_IF_ERROR(SaveStateInternal(ctx, &writer));
    }
    TF_RETURN_IF_ERROR(bundle_writer.Finish());
    return Status::OK();
  }

  // Restores the state of this iterator.
  virtual Status RestoreState(OpKernelContext* ctx, StringPiece& path) {
    if (!(ctx->env()->FileExists(MetaFilename(path)).ok())) {
      return errors::NotFound(
          "Failed to restore Iterator state. No file found at ",
          MetaFilename(path));
    }
    BundleReader bundle_reader(ctx->env(), path);
    if (bundle_reader.Contains(kIteratorExhausted)) {
      LOG(INFO) << "Iterator exhausted. Nothing to restore.";
      is_exhausted_ = true;
      return Status::OK();
    } else {
      IteratorBundleReader reader(&bundle_reader);
      return RestoreStateInternal(ctx, &reader);
    }
  }

 protected:
  class IteratorBundleReader {
   public:
    IteratorBundleReader(BundleReader* bundle_reader)
        : bundle_reader_(bundle_reader) {}

    // Reads a scalar value.
    template <typename T>
    Status ReadScalar(T* val, const string& key) {
      Tensor val_t = Tensor(DataTypeToEnum<T>::v(), TensorShape({}));
      TF_RETURN_IF_ERROR(Lookup(StringPiece(key), &val_t));
      *val = val_t.scalar<T>()();
      return Status::OK();
    }

    // Restores the state of a parent iterator recursively.
    Status RestoreParentState(OpKernelContext* ctx,
                              const std::unique_ptr<IteratorBase>& parent) {
      return parent->RestoreStateInternal(ctx, this);
    }

   private:
    Status Lookup(StringPiece key, Tensor* val) {
      return bundle_reader_->Lookup(key, val);
    }

    BundleReader* bundle_reader_;
  };

  class IteratorBundleWriter {
   public:
    IteratorBundleWriter(BundleWriter* bundle_writer)
        : bundle_writer_(bundle_writer) {}

    // Writes a scalar value.
    template <typename T>
    Status WriteScalar(const T val, const string& key) {
      Tensor val_t = Tensor(DataTypeToEnum<T>::v(), TensorShape({}));
      val_t.scalar<T>()() = val;
      TF_RETURN_IF_ERROR(Add(StringPiece(key), val_t));
      return Status::OK();
    }

    // Saves the state of a parent iterator recursively.
    Status SaveParentState(OpKernelContext* ctx,
                           const std::unique_ptr<IteratorBase>& parent) {
      return parent->SaveStateInternal(ctx, this);
    }

   private:
    Status Add(StringPiece key, const Tensor& val) {
      return bundle_writer_->Add(key, val);
    }

    BundleWriter* bundle_writer_;
  };

  // Saves the state of this iterator.
  // Note: Contents written to `writer` may not get flushed to disk
  // until the call to `SaveState` in the leaf iterator is finished.
  // Must be overridden by sub-classes.
  virtual Status SaveStateInternal(OpKernelContext* ctx,
                                   IteratorBundleWriter* writer) {
    return errors::Unimplemented("SaveState not implemented.");
  }

  // Restores the state of this iterator.
  //
  // Must be overridden by sub-classes.
  virtual Status RestoreStateInternal(OpKernelContext* ctx,
                                      IteratorBundleReader* reader) {
    return errors::Unimplemented("RestoreState not implemented");
  }

  bool is_exhausted_ = false;  // Whether the iterator has been exhausted.

 private:
  static const char kIteratorExhausted[];
};

// Represents a (potentially infinite) range of outputs, where each
// output is a tuple of tensors.
class DatasetBase : public core::RefCounted {
 public:
  // Returns a new iterator for iterating over the range of elements in
  // this dataset.
  //
  // This method may be called multiple times on the same instance,
  // and the resulting iterators will have distinct state. Each
  // iterator will traverse all elements in this dataset from the
  // start.
  //
  // Ownership of the created iterator will be transferred to the caller.
  //
  // The prefix identifies the sequence of iterators leading up to the newly
  // created iterator.
  virtual std::unique_ptr<IteratorBase> MakeIterator(
      const string& prefix) const = 0;

  // Returns a vector of DataType values, representing the respective
  // element types of each tuple component in the outputs of this
  // dataset.
  virtual const DataTypeVector& output_dtypes() const = 0;

  // Returns a vector of tensor shapes, representing the respective
  // (and possibly partially defined) shapes of each tuple component
  // in the outputs of this dataset.
  virtual const std::vector<PartialTensorShape>& output_shapes() const = 0;

  // A human-readable debug string for this dataset.
  virtual string DebugString() = 0;
};

// Represents an iterator that is associated with a particular parent dataset.
template <class DatasetType>
class DatasetIterator : public IteratorBase {
 public:
  struct Params {
    // Owns one reference on the shared dataset resource.
    const DatasetType* dataset;

    // Identifies the sequence of iterators leading up to to this iterator.
    const string prefix;
  };

  explicit DatasetIterator(const Params& params) : params_(params) {
    params_.dataset->Ref();
  }

  ~DatasetIterator() override { params_.dataset->Unref(); }

  // The dataset from which this iterator was created.
  const DatasetType* dataset() const { return params_.dataset; }

  // The sequence of iterators leading up to this iterator.
  const string prefix() const { return params_.prefix; }

  const DataTypeVector& output_dtypes() const override {
    return params_.dataset->output_dtypes();
  }

  const std::vector<PartialTensorShape>& output_shapes() const override {
    return params_.dataset->output_shapes();
  }

  Status GetNext(IteratorContext* ctx, std::vector<Tensor>* out_tensors,
                 bool* end_of_sequence) final {
    port::Tracing::TraceMe activity(params_.prefix);
    if (is_exhausted_) {
      *end_of_sequence = true;
      return Status::OK();
    }
    return GetNextInternal(ctx, out_tensors, end_of_sequence);
  }

  // Internal implementation of GetNext that is wrapped in tracing logic.
  virtual Status GetNextInternal(IteratorContext* ctx,
                                 std::vector<Tensor>* out_tensors,
                                 bool* end_of_sequence) = 0;

 protected:
  string full_name(const string& name) {
    return strings::StrCat(prefix(), ":", name);
  }

 private:
  Params params_;
};

// Encapsulates the work required to plug a DatasetBase into the core TensorFlow
// graph execution engine.
class DatasetOpKernel : public OpKernel {
 public:
  DatasetOpKernel(OpKernelConstruction* ctx) : OpKernel(ctx) {}
  void Compute(OpKernelContext* ctx) final;

 protected:
  // Subclasses should implement this method. It will be called during Compute
  // execution.
  virtual void MakeDataset(OpKernelContext* ctx, DatasetBase** output) = 0;

  template <typename T>
  Status ParseScalarArgument(OpKernelContext* ctx,
                             const StringPiece& argument_name, T* output) {
    const Tensor* argument_t;
    TF_RETURN_IF_ERROR(ctx->input(argument_name, &argument_t));
    if (!TensorShapeUtils::IsScalar(argument_t->shape())) {
      return errors::InvalidArgument(argument_name, " must be a scalar");
    }
    *output = argument_t->scalar<T>()();
    return Status::OK();
  }
};

// Encapsulates the work required to plug unary Datasets into the core
// TensorFlow graph execution engine.
class UnaryDatasetOpKernel : public DatasetOpKernel {
 public:
  UnaryDatasetOpKernel(OpKernelConstruction* ctx) : DatasetOpKernel(ctx) {}

 protected:
  void MakeDataset(OpKernelContext* ctx, DatasetBase** output) final;
  virtual void MakeDataset(OpKernelContext* ctx, DatasetBase* input,
                           DatasetBase** output) = 0;
};

// Encapsulates the work required to plug binary Datasets into the core
// TensorFlow graph execution engine.
class BinaryDatasetOpKernel : public DatasetOpKernel {
 public:
  BinaryDatasetOpKernel(OpKernelConstruction* ctx) : DatasetOpKernel(ctx) {}

 protected:
  void MakeDataset(OpKernelContext* ctx, DatasetBase** output) final;
  virtual void MakeDataset(OpKernelContext* ctx, DatasetBase* input,
                           DatasetBase* another_input,
                           DatasetBase** output) = 0;
};

// Validates and extracts a `DatasetBase` object from `tensor`.
//
// `tensor` must have been written by a call to SetVariantTensorToDataset().
//
// The retrieved pointer is a borrowed reference to the dataset, which is owned
// by the tensor. The consumer must either acquire its own reference to the
// dataset by calling `(*out_dataset)->Ref()`, or ensure that `tensor` is not
// destroyed or mutated while the retrieved pointer is in use.
Status GetDatasetFromVariantTensor(const Tensor& tensor,
                                   DatasetBase** out_dataset);

// Stores a `DatasetBase` object in `tensor`.
//
// The ownership of `dataset` is transferred to `tensor`.
Status StoreDatasetInVariantTensor(DatasetBase* dataset, Tensor* tensor);

}  // namespace tensorflow

#endif  // THIRD_PARTY_TENSORFLOW_CORE_KERNELS_DATASET_H_
