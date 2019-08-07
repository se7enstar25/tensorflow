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
#include "tensorflow/core/kernels/data/text_line_dataset_op.h"

#include "tensorflow/core/common_runtime/metrics.h"
#include "tensorflow/core/framework/partial_tensor_shape.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/kernels/data/name_utils.h"
#include "tensorflow/core/lib/io/buffered_inputstream.h"
#include "tensorflow/core/lib/io/inputbuffer.h"
#include "tensorflow/core/lib/io/random_inputstream.h"
#include "tensorflow/core/lib/io/zlib_compression_options.h"
#include "tensorflow/core/lib/io/zlib_inputstream.h"

namespace tensorflow {
namespace data {

/* static */ constexpr const char* const TextLineDatasetOp::kDatasetType;
/* static */ constexpr const char* const TextLineDatasetOp::kFileNames;
/* static */ constexpr const char* const TextLineDatasetOp::kCompressionType;
/* static */ constexpr const char* const TextLineDatasetOp::kBufferSize;

constexpr char kZLIB[] = "ZLIB";
constexpr char kGZIP[] = "GZIP";
constexpr char kCurrentFileIndex[] = "current_file_index";
constexpr char kCurrentPos[] = "current_pos";

class TextLineDatasetOp::Dataset : public DatasetBase {
 public:
  Dataset(OpKernelContext* ctx, std::vector<string> filenames,
          const string& compression_type,
          const io::ZlibCompressionOptions& options)
      : DatasetBase(DatasetContext(ctx)),
        filenames_(std::move(filenames)),
        compression_type_(compression_type),
        use_compression_(!compression_type.empty()),
        options_(options) {}

  std::unique_ptr<IteratorBase> MakeIteratorInternal(
      const string& prefix) const override {
    return absl::make_unique<Iterator>(Iterator::Params{
        this,
        name_utils::IteratorPrefix(TextLineDatasetOp::kDatasetType, prefix)});
  }

  const DataTypeVector& output_dtypes() const override {
    static DataTypeVector* dtypes = new DataTypeVector({DT_STRING});
    return *dtypes;
  }

  const std::vector<PartialTensorShape>& output_shapes() const override {
    static std::vector<PartialTensorShape>* shapes =
        new std::vector<PartialTensorShape>({{}});
    return *shapes;
  }

  string DebugString() const override {
    return name_utils::DatasetDebugString(kDatasetType);
  }

 protected:
  Status AsGraphDefInternal(SerializationContext* ctx,
                            DatasetGraphDefBuilder* b,
                            Node** output) const override {
    Node* filenames = nullptr;
    Node* compression_type = nullptr;
    Node* buffer_size = nullptr;
    TF_RETURN_IF_ERROR(b->AddVector(filenames_, &filenames));
    TF_RETURN_IF_ERROR(b->AddScalar(compression_type_, &compression_type));
    TF_RETURN_IF_ERROR(b->AddScalar(options_.input_buffer_size, &buffer_size));
    TF_RETURN_IF_ERROR(b->AddDataset(
        this, {filenames, compression_type, buffer_size}, output));
    return Status::OK();
  }

 private:
  class Iterator : public DatasetIterator<Dataset> {
   public:
    explicit Iterator(const Params& params)
        : DatasetIterator<Dataset>(params) {}

    Status GetNextInternal(IteratorContext* ctx,
                           std::vector<Tensor>* out_tensors,
                           bool* end_of_sequence) override {
      mutex_lock l(mu_);
      do {
        // We are currently processing a file, so try to read the next line.
        if (buffered_input_stream_) {
          string line_contents;
          Status s = buffered_input_stream_->ReadLine(&line_contents);

          if (s.ok()) {
            // Produce the line as output.
            metrics::RecordTFDataBytesRead(
                name_utils::OpName(TextLineDatasetOp::kDatasetType),
                line_contents.size());
            out_tensors->emplace_back(ctx->allocator({}), DT_STRING,
                                      TensorShape({}));
            out_tensors->back().scalar<tstring>()() = std::move(line_contents);
            *end_of_sequence = false;
            return Status::OK();
          } else if (!errors::IsOutOfRange(s)) {
            // Report non-EOF errors to the caller.
            return s;
          }
          // We have reached the end of the current file, so maybe
          // move on to next file.
          ResetStreamsLocked();
          ++current_file_index_;
        }

        // Iteration ends when there are no more files to process.
        if (current_file_index_ == dataset()->filenames_.size()) {
          *end_of_sequence = true;
          return Status::OK();
        }

        TF_RETURN_IF_ERROR(SetupStreamsLocked(ctx->env()));
      } while (true);
    }

   protected:
    std::shared_ptr<model::Node> CreateNode(
        IteratorContext* ctx, model::Node::Args args) const override {
      return model::MakeSourceNode(std::move(args));
    }

    Status SaveInternal(IteratorStateWriter* writer) override {
      mutex_lock l(mu_);
      TF_RETURN_IF_ERROR(writer->WriteScalar(full_name(kCurrentFileIndex),
                                             current_file_index_));
      // `buffered_input_stream_` is empty if
      // 1. GetNext has not been called even once.
      // 2. All files have been read and iterator has been exhausted.
      if (buffered_input_stream_) {
        TF_RETURN_IF_ERROR(writer->WriteScalar(full_name(kCurrentPos),
                                               buffered_input_stream_->Tell()));
      }
      return Status::OK();
    }

    Status RestoreInternal(IteratorContext* ctx,
                           IteratorStateReader* reader) override {
      mutex_lock l(mu_);
      ResetStreamsLocked();
      int64 current_file_index;
      TF_RETURN_IF_ERROR(reader->ReadScalar(full_name(kCurrentFileIndex),
                                            &current_file_index));
      current_file_index_ = size_t(current_file_index);
      // The key "current_pos" is written only if the iterator was saved
      // with an open file.
      if (reader->Contains(full_name(kCurrentPos))) {
        int64 current_pos;
        TF_RETURN_IF_ERROR(
            reader->ReadScalar(full_name(kCurrentPos), &current_pos));

        TF_RETURN_IF_ERROR(SetupStreamsLocked(ctx->env()));
        TF_RETURN_IF_ERROR(buffered_input_stream_->Seek(current_pos));
      }
      return Status::OK();
    }

   private:
    // Sets up reader streams to read from the file at `current_file_index_`.
    Status SetupStreamsLocked(Env* env) EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      if (current_file_index_ >= dataset()->filenames_.size()) {
        return errors::InvalidArgument(
            "current_file_index_:", current_file_index_,
            " >= filenames_.size():", dataset()->filenames_.size());
      }

      // Actually move on to next file.
      TF_RETURN_IF_ERROR(env->NewRandomAccessFile(
          dataset()->filenames_[current_file_index_], &file_));
      input_stream_ =
          absl::make_unique<io::RandomAccessInputStream>(file_.get(), false);

      if (dataset()->use_compression_) {
        zlib_input_stream_ = absl::make_unique<io::ZlibInputStream>(
            input_stream_.get(), dataset()->options_.input_buffer_size,
            dataset()->options_.input_buffer_size, dataset()->options_);
        buffered_input_stream_ = absl::make_unique<io::BufferedInputStream>(
            zlib_input_stream_.get(), dataset()->options_.input_buffer_size,
            false);
      } else {
        buffered_input_stream_ = absl::make_unique<io::BufferedInputStream>(
            input_stream_.get(), dataset()->options_.input_buffer_size, false);
      }
      return Status::OK();
    }

    // Resets all reader streams.
    void ResetStreamsLocked() EXCLUSIVE_LOCKS_REQUIRED(mu_) {
      input_stream_.reset();
      zlib_input_stream_.reset();
      buffered_input_stream_.reset();
      file_.reset();
    }

    mutex mu_;
    std::unique_ptr<io::RandomAccessInputStream> input_stream_ GUARDED_BY(mu_);
    std::unique_ptr<io::ZlibInputStream> zlib_input_stream_ GUARDED_BY(mu_);
    std::unique_ptr<io::BufferedInputStream> buffered_input_stream_
        GUARDED_BY(mu_);
    size_t current_file_index_ GUARDED_BY(mu_) = 0;
    std::unique_ptr<RandomAccessFile> file_
        GUARDED_BY(mu_);  // must outlive input_stream_
  };

  const std::vector<string> filenames_;
  const string compression_type_;
  const bool use_compression_;
  const io::ZlibCompressionOptions options_;
};

TextLineDatasetOp::TextLineDatasetOp(OpKernelConstruction* ctx)
    : DatasetOpKernel(ctx) {}

void TextLineDatasetOp::MakeDataset(OpKernelContext* ctx,
                                    DatasetBase** output) {
  const Tensor* filenames_tensor;
  OP_REQUIRES_OK(ctx, ctx->input(kFileNames, &filenames_tensor));
  OP_REQUIRES(
      ctx, filenames_tensor->dims() <= 1,
      errors::InvalidArgument("`filenames` must be a scalar or a vector."));

  string compression_type;
  OP_REQUIRES_OK(ctx, ParseScalarArgument<string>(ctx, kCompressionType,
                                                  &compression_type));

  int64 buffer_size = -1;
  OP_REQUIRES_OK(ctx,
                 ParseScalarArgument<int64>(ctx, kBufferSize, &buffer_size));
  OP_REQUIRES(
      ctx, buffer_size >= 0,
      errors::InvalidArgument("`buffer_size` must be >= 0 (0 == default)"));

  io::ZlibCompressionOptions zlib_compression_options =
      io::ZlibCompressionOptions::DEFAULT();
  if (compression_type == kZLIB) {
    zlib_compression_options = io::ZlibCompressionOptions::DEFAULT();
  } else if (compression_type == kGZIP) {
    zlib_compression_options = io::ZlibCompressionOptions::GZIP();
  } else {
    OP_REQUIRES(ctx, compression_type.empty(),
                errors::InvalidArgument("Unsupported compression_type."));
  }

  if (buffer_size != 0) {
    // Set the override size.
    zlib_compression_options.input_buffer_size = buffer_size;
  }

  std::vector<string> filenames;
  filenames.reserve(filenames_tensor->NumElements());
  for (int i = 0; i < filenames_tensor->NumElements(); ++i) {
    filenames.push_back(filenames_tensor->flat<tstring>()(i));
  }

  *output = new Dataset(ctx, std::move(filenames), compression_type,
                        zlib_compression_options);
}

namespace {
REGISTER_KERNEL_BUILDER(Name("TextLineDataset").Device(DEVICE_CPU),
                        TextLineDatasetOp);
}  // namespace
}  // namespace data
}  // namespace tensorflow
