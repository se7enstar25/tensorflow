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
#include "tensorflow/contrib/lite/toco/tflite/operator.h"

#include "tensorflow/contrib/lite/toco/tflite/builtin_operator.h"
#include "tensorflow/contrib/lite/toco/tflite/custom_operator.h"
#include "tensorflow/contrib/lite/toco/tflite/simple_operator.h"
#include "tensorflow/contrib/lite/toco/tflite/types.h"

#include "tensorflow/core/framework/attr_value.pb.h"
#include "tensorflow/core/framework/node_def.pb.h"

namespace toco {

namespace tflite {

class AveragePool
    : public BuiltinOperator<AveragePoolOperator, ::tflite::Pool2DOptions,
                             ::tflite::BuiltinOptions_Pool2DOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;

  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto padding = Padding::Serialize(op.padding.type);
    auto activation_function =
        ActivationFunction::Serialize(op.fused_activation_function);
    return ::tflite::CreatePool2DOptions(*builder, padding, op.stride_width,
                                         op.stride_height, op.kwidth,
                                         op.kheight, activation_function);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->padding.type = Padding::Deserialize(options.padding());
    op->stride_width = options.stride_w();
    op->stride_height = options.stride_h();
    op->kwidth = options.filter_width();
    op->kheight = options.filter_height();
    op->fused_activation_function =
        ActivationFunction::Deserialize(options.fused_activation_function());
  }
};

class Convolution
    : public BuiltinOperator<ConvOperator, ::tflite::Conv2DOptions,
                             ::tflite::BuiltinOptions_Conv2DOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;

  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto padding = Padding::Serialize(op.padding.type);
    auto activation_function =
        ActivationFunction::Serialize(op.fused_activation_function);
    return ::tflite::CreateConv2DOptions(*builder, padding, op.stride_width,
                                         op.stride_height, activation_function);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->padding.type = Padding::Deserialize(options.padding());
    op->stride_width = options.stride_w();
    op->stride_height = options.stride_h();
    op->fused_activation_function =
        ActivationFunction::Deserialize(options.fused_activation_function());
  }
};

class DepthwiseConvolution
    : public BuiltinOperator<DepthwiseConvOperator,
                             ::tflite::DepthwiseConv2DOptions,
                             ::tflite::BuiltinOptions_DepthwiseConv2DOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;

  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto padding = Padding::Serialize(op.padding.type);
    auto activation_function =
        ActivationFunction::Serialize(op.fused_activation_function);
    return ::tflite::CreateDepthwiseConv2DOptions(
        *builder, padding, op.stride_width, op.stride_height,
        op.depth_multiplier, activation_function);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->padding.type = Padding::Deserialize(options.padding());
    op->stride_width = options.stride_w();
    op->stride_height = options.stride_h();
    op->depth_multiplier = options.depth_multiplier();
    op->fused_activation_function =
        ActivationFunction::Deserialize(options.fused_activation_function());
  }
};

class Add : public BuiltinOperator<AddOperator, ::tflite::AddOptions,
                                   ::tflite::BuiltinOptions_AddOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;

  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto activation_function =
        ActivationFunction::Serialize(op.fused_activation_function);
    return ::tflite::CreateAddOptions(*builder, activation_function);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->fused_activation_function =
        ActivationFunction::Deserialize(options.fused_activation_function());
  }
};

class SpaceToBatchND
    : public BuiltinOperator<SpaceToBatchNDOperator,
                             ::tflite::SpaceToBatchNDOptions,
                             ::tflite::BuiltinOptions_SpaceToBatchNDOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;

  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto block_shape = builder->CreateVector(op.block_shape);
    auto before_paddings = builder->CreateVector(op.before_paddings);
    auto after_paddings = builder->CreateVector(op.after_paddings);
    return ::tflite::CreateSpaceToBatchNDOptions(
        *builder, block_shape, before_paddings, after_paddings);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->block_shape.insert(op->block_shape.end(),
                           options.block_shape()->begin(),
                           options.block_shape()->end());
    op->before_paddings.insert(op->before_paddings.end(),
                               options.before_paddings()->begin(),
                               options.before_paddings()->end());
    op->after_paddings.insert(op->after_paddings.end(),
                              options.after_paddings()->begin(),
                              options.after_paddings()->end());
  }
};

class BatchToSpaceND
    : public BuiltinOperator<BatchToSpaceNDOperator,
                             ::tflite::BatchToSpaceNDOptions,
                             ::tflite::BuiltinOptions_BatchToSpaceNDOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;

  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto block_shape = builder->CreateVector(op.block_shape);
    auto before_crops = builder->CreateVector(op.before_crops);
    auto after_crops = builder->CreateVector(op.after_crops);
    return ::tflite::CreateBatchToSpaceNDOptions(*builder, block_shape,
                                                 before_crops, after_crops);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->block_shape.insert(op->block_shape.end(),
                           options.block_shape()->begin(),
                           options.block_shape()->end());
    op->before_crops.insert(op->before_crops.end(),
                            options.before_crops()->begin(),
                            options.before_crops()->end());
    op->after_crops.insert(op->after_crops.end(),
                           options.after_crops()->begin(),
                           options.after_crops()->end());
  }
};

class Cast : public CustomOperator<CastOperator> {
 public:
  using CustomOperator::CustomOperator;
  void WriteOptions(const TocoOperator& op,
                    flexbuffers::Builder* fbb) const override {
    fbb->Int("src_data_type", DataType::Serialize(op.src_data_type));
    fbb->Int("dst_data_type", DataType::Serialize(op.dst_data_type));
  }
  void ReadOptions(const flexbuffers::Map& m, TocoOperator* op) const override {
    op->src_data_type = DataType::Deserialize(m["src_data_type"].AsInt64());
    op->dst_data_type = DataType::Deserialize(m["dst_data_type"].AsInt64());
  }
};

class Concatenation
    : public BuiltinOperator<ConcatenationOperator,
                             ::tflite::ConcatenationOptions,
                             ::tflite::BuiltinOptions_ConcatenationOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;
  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    return ::tflite::CreateConcatenationOptions(*builder, op.axis);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->axis = options.axis();
  }
};

class DepthToSpace : public CustomOperator<DepthToSpaceOperator> {
 public:
  using CustomOperator::CustomOperator;
  void WriteOptions(const TocoOperator& op,
                    flexbuffers::Builder* fbb) const override {
    fbb->Int("block_size", op.block_size);
  }
  void ReadOptions(const flexbuffers::Map& m, TocoOperator* op) const override {
    op->block_size = m["block_size"].AsInt64();
  }
};

class FakeQuant : public CustomOperator<FakeQuantOperator> {
 public:
  using CustomOperator::CustomOperator;
  void WriteOptions(const TocoOperator& op,
                    flexbuffers::Builder* fbb) const override {
    fbb->Float("min", op.minmax->min);
    fbb->Float("max", op.minmax->max);
  }
  void ReadOptions(const flexbuffers::Map& m, TocoOperator* op) const override {
    auto* minmax = new MinMax;
    minmax->min = m["min"].AsFloat();
    minmax->max = m["max"].AsFloat();
    op->minmax.reset(minmax);
  }
};

class FullyConnected
    : public BuiltinOperator<FullyConnectedOperator,
                             ::tflite::FullyConnectedOptions,
                             ::tflite::BuiltinOptions_FullyConnectedOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;
  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto activation_function =
        ActivationFunction::Serialize(op.fused_activation_function);
    return ::tflite::CreateFullyConnectedOptions(*builder, activation_function);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->fused_activation_function =
        ActivationFunction::Deserialize(options.fused_activation_function());
  }
};

class Gather : public BuiltinOperator<GatherOperator, ::tflite::GatherOptions,
                                      ::tflite::BuiltinOptions_GatherOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;
  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    return ::tflite::CreateGatherOptions(*builder, op.axis);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->axis = options.axis();
  }
};

class Svdf : public BuiltinOperator<SvdfOperator, ::tflite::SVDFOptions,
                                    ::tflite::BuiltinOptions_SVDFOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;
  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto activation_function =
        ActivationFunction::Serialize(op.fused_activation_function);
    return ::tflite::CreateSVDFOptions(*builder, op.rank, activation_function);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->fused_activation_function =
        ActivationFunction::Deserialize(options.fused_activation_function());
    op->rank = options.rank();
  }
};

class L2Normalization
    : public BuiltinOperator<L2NormalizationOperator, ::tflite::L2NormOptions,
                             ::tflite::BuiltinOptions_L2NormOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;
  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto activation_function =
        ActivationFunction::Serialize(op.fused_activation_function);
    return ::tflite::CreateL2NormOptions(*builder, activation_function);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->fused_activation_function =
        ActivationFunction::Deserialize(options.fused_activation_function());
  }
};

class L2Pool : public BuiltinOperator<L2PoolOperator, ::tflite::Pool2DOptions,
                                      ::tflite::BuiltinOptions_Pool2DOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;
  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto padding = Padding::Serialize(op.padding.type);
    auto activation_function =
        ActivationFunction::Serialize(op.fused_activation_function);
    return ::tflite::CreatePool2DOptions(*builder, padding, op.stride_width,
                                         op.stride_height, op.kwidth,
                                         op.kheight, activation_function);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->padding.type = Padding::Deserialize(options.padding());
    op->stride_width = options.stride_w();
    op->stride_height = options.stride_h();
    op->kwidth = options.filter_width();
    op->kheight = options.filter_height();
    op->fused_activation_function =
        ActivationFunction::Deserialize(options.fused_activation_function());
  }
};

class LocalResponseNormalization
    : public BuiltinOperator<
          LocalResponseNormalizationOperator,
          ::tflite::LocalResponseNormalizationOptions,
          ::tflite::BuiltinOptions_LocalResponseNormalizationOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;
  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    return ::tflite::CreateLocalResponseNormalizationOptions(
        *builder, op.range, op.bias, op.alpha, op.beta);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->range = options.radius();
    op->bias = options.bias();
    op->alpha = options.alpha();
    op->beta = options.beta();
  }
};

class MaxPool : public BuiltinOperator<MaxPoolOperator, ::tflite::Pool2DOptions,
                                       ::tflite::BuiltinOptions_Pool2DOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;
  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto padding = Padding::Serialize(op.padding.type);
    auto activation_function =
        ActivationFunction::Serialize(op.fused_activation_function);
    return ::tflite::CreatePool2DOptions(*builder, padding, op.stride_width,
                                         op.stride_height, op.kwidth,
                                         op.kheight, activation_function);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->padding.type = Padding::Deserialize(options.padding());
    op->stride_width = options.stride_w();
    op->stride_height = options.stride_h();
    op->kwidth = options.filter_width();
    op->kheight = options.filter_height();
    op->fused_activation_function =
        ActivationFunction::Deserialize(options.fused_activation_function());
  }
};

class Mul : public BuiltinOperator<MulOperator, ::tflite::MulOptions,
                                   ::tflite::BuiltinOptions_MulOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;

  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto activation_function =
        ActivationFunction::Serialize(op.fused_activation_function);
    return ::tflite::CreateMulOptions(*builder, activation_function);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->fused_activation_function =
        ActivationFunction::Deserialize(options.fused_activation_function());
  }
};

class Pad : public BuiltinOperator<PadOperator, ::tflite::PadOptions,
                                   ::tflite::BuiltinOptions_PadOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;

  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto before_padding = builder->CreateVector(op.left_padding);
    auto after_padding = builder->CreateVector(op.right_padding);
    return ::tflite::CreatePadOptions(*builder, before_padding, after_padding);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->left_padding.insert(op->left_padding.end(),
                            options.before_padding()->begin(),
                            options.before_padding()->end());
    op->right_padding.insert(op->right_padding.end(),
                             options.after_padding()->begin(),
                             options.after_padding()->end());
  }
};

class Reshape
    : public BuiltinOperator<TensorFlowReshapeOperator,
                             ::tflite::ReshapeOptions,
                             ::tflite::BuiltinOptions_ReshapeOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;

  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    return ::tflite::CreateReshapeOptions(*builder,
                                          builder->CreateVector(op.shape));
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->shape.insert(op->shape.end(), options.new_shape()->begin(),
                     options.new_shape()->end());
  }
};

class Softmax
    : public BuiltinOperator<SoftmaxOperator, ::tflite::SoftmaxOptions,
                             ::tflite::BuiltinOptions_SoftmaxOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;
  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    return ::tflite::CreateSoftmaxOptions(*builder, op.beta);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->beta = options.beta();
  }
};

class SpaceToDepth
    : public BuiltinOperator<SpaceToDepthOperator,
                             ::tflite::SpaceToDepthOptions,
                             ::tflite::BuiltinOptions_SpaceToDepthOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;
  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    return ::tflite::CreateSpaceToDepthOptions(*builder, op.block_size);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->block_size = options.block_size();
  }
};

class Transpose
    : public BuiltinOperator<TransposeOperator, ::tflite::TransposeOptions,
                             ::tflite::BuiltinOptions_TransposeOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;
  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    return ::tflite::CreateTransposeOptions(*builder,
                                            builder->CreateVector(op.perm));
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->perm.insert(op->perm.end(), options.perm()->begin(),
                    options.perm()->end());
  }
};

class Mean : public BuiltinOperator<MeanOperator, ::tflite::MeanOptions,
                                    ::tflite::BuiltinOptions_MeanOptions> {
 public:
  using BuiltinOperator::BuiltinOperator;
  flatbuffers::Offset<TfLiteOptions> WriteOptions(
      const TocoOperator& op,
      flatbuffers::FlatBufferBuilder* builder) const override {
    auto axis = builder->CreateVector(op.axis);
    return ::tflite::CreateMeanOptions(*builder, axis, op.keep_dims);
  }

  void ReadOptions(const TfLiteOptions& options,
                   TocoOperator* op) const override {
    op->axis.insert(op->axis.end(), options.axis()->begin(),
                    options.axis()->end());
    op->keep_dims = options.keep_dims();
  }
};

class Split : public CustomOperator<TensorFlowSplitOperator> {
 public:
  using CustomOperator::CustomOperator;
  void WriteOptions(const TocoOperator& op,
                    flexbuffers::Builder* fbb) const override {
    fbb->Int("num_split", op.num_split);
  }
  void ReadOptions(const flexbuffers::Map& m, TocoOperator* op) const override {
    op->num_split = m["num_split"].AsInt64();
  }
};

class TensorFlowUnsupported : public BaseOperator {
 public:
  using BaseOperator::BaseOperator;

  Options Serialize(const Operator& op,
                    flatbuffers::FlatBufferBuilder* builder) const override {
    auto fbb =
        WriteOptions(static_cast<const TensorFlowUnsupportedOperator&>(op));
    if (fbb) {
      return Options::Custom(builder->CreateVector(fbb->GetBuffer()));
    } else {
      return Options::Custom(0);
    }
  }

  std::unique_ptr<Operator> Deserialize(
      const BuiltinOptions* builtin_options,
      const CustomOptions* custom_options) const override {
    auto op = absl::make_unique<TensorFlowUnsupportedOperator>();
    if (custom_options) {
      auto flexbuffer_map =
          flexbuffers::GetRoot(custom_options->data(), custom_options->size())
              .AsMap();
      ReadOptions(flexbuffer_map, op.get());
    }
    return std::unique_ptr<Operator>(op.release());
  }

  std::unique_ptr<flexbuffers::Builder> WriteOptions(
      const TensorFlowUnsupportedOperator& op) const {
    auto fbb = absl::make_unique<flexbuffers::Builder>();

    ::tensorflow::NodeDef node_def;
    if (!node_def.ParseFromString(op.tensorflow_node_def)) {
      LOG(ERROR) << "Failed to parse TensorFlow NodeDef";
      return std::unique_ptr<flexbuffers::Builder>();
    }

    bool has_valid_attr = false;
    size_t map_start = fbb->StartMap();
    for (const auto& pair : node_def.attr()) {
      const char* key = pair.first.c_str();
      const auto& attr = pair.second;
      switch (attr.value_case()) {
        case ::tensorflow::AttrValue::kS:
          fbb->String(key, attr.s());
          has_valid_attr = true;
          break;
        case ::tensorflow::AttrValue::kI:
          fbb->Int(key, attr.i());
          has_valid_attr = true;
          break;
        case ::tensorflow::AttrValue::kF:
          fbb->Float(key, attr.f());
          has_valid_attr = true;
          break;
        case ::tensorflow::AttrValue::kB:
          fbb->Bool(key, attr.b());
          has_valid_attr = true;
          break;
        default:
          LOG(WARNING) << "Ignoring unsupported attribute type with key '"
                       << key << "'";
          break;
      }
    }
    if (!has_valid_attr) {
      return std::unique_ptr<flexbuffers::Builder>();
    }
    fbb->EndMap(map_start);
    fbb->Finish();
    return std::unique_ptr<flexbuffers::Builder>(fbb.release());
  }

  void ReadOptions(const flexbuffers::Map& m,
                   TensorFlowUnsupportedOperator* op) const {
    ::tensorflow::NodeDef node_def;
    auto attr = node_def.mutable_attr();

    const auto& keys = m.Keys();
    for (size_t i = 0; i < keys.size(); ++i) {
      const auto key = keys[i].AsKey();
      const auto& value = m[key];
      switch (value.GetType()) {
        case flexbuffers::TYPE_STRING:
          (*attr)[key].set_s(value.AsString().c_str());
          break;
        case flexbuffers::TYPE_INT:
          (*attr)[key].set_i(value.AsInt64());
          break;
        case flexbuffers::TYPE_FLOAT:
          (*attr)[key].set_f(value.AsFloat());
          break;
        case flexbuffers::TYPE_BOOL:
          (*attr)[key].set_b(value.AsBool());
          break;
        default:
          LOG(WARNING) << "Ignoring unsupported attribute type with key '"
                       << key << "'";
          break;
      }
    }
    node_def.SerializeToString(&op->tensorflow_node_def);
  }
};

namespace {
// Build a vector containing all the known operators.
std::vector<std::unique_ptr<BaseOperator>> BuildOperatorList() {
  std::vector<std::unique_ptr<BaseOperator>> ops;

  // Builtin Operators.
  ops.emplace_back(new Add(::tflite::BuiltinOperator_ADD, OperatorType::kAdd));
  ops.emplace_back(new AveragePool(::tflite::BuiltinOperator_AVERAGE_POOL_2D,
                                   OperatorType::kAveragePool));
  ops.emplace_back(
      new SpaceToBatchND(::tflite::BuiltinOperator_SPACE_TO_BATCH_ND,
                         OperatorType::kSpaceToBatchND));
  ops.emplace_back(
      new BatchToSpaceND(::tflite::BuiltinOperator_BATCH_TO_SPACE_ND,
                         OperatorType::kBatchToSpaceND));
  ops.emplace_back(new Concatenation(::tflite::BuiltinOperator_CONCATENATION,
                                     OperatorType::kConcatenation));
  ops.emplace_back(
      new Convolution(::tflite::BuiltinOperator_CONV_2D, OperatorType::kConv));
  ops.emplace_back(
      new DepthwiseConvolution(::tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
                               OperatorType::kDepthwiseConv));
  ops.emplace_back(new FullyConnected(::tflite::BuiltinOperator_FULLY_CONNECTED,
                                      OperatorType::kFullyConnected));
  ops.emplace_back(
      new Gather(::tflite::BuiltinOperator_GATHER, OperatorType::kGather));
  ops.emplace_back(
      new L2Normalization(::tflite::BuiltinOperator_L2_NORMALIZATION,
                          OperatorType::kL2Normalization));
  ops.emplace_back(
      new L2Pool(::tflite::BuiltinOperator_L2_POOL_2D, OperatorType::kL2Pool));
  ops.emplace_back(new LocalResponseNormalization(
      ::tflite::BuiltinOperator_LOCAL_RESPONSE_NORMALIZATION,
      OperatorType::kLocalResponseNormalization));
  ops.emplace_back(new MaxPool(::tflite::BuiltinOperator_MAX_POOL_2D,
                               OperatorType::kMaxPool));
  ops.emplace_back(new Mul(::tflite::BuiltinOperator_MUL, OperatorType::kMul));
  ops.emplace_back(new Pad(::tflite::BuiltinOperator_PAD, OperatorType::kPad));
  ops.emplace_back(new Reshape(::tflite::BuiltinOperator_RESHAPE,
                               OperatorType::kTensorFlowReshape));
  ops.emplace_back(
      new Softmax(::tflite::BuiltinOperator_SOFTMAX, OperatorType::kSoftmax));
  ops.emplace_back(new SpaceToDepth(::tflite::BuiltinOperator_SPACE_TO_DEPTH,
                                    OperatorType::kSpaceToDepth));
  ops.emplace_back(
      new Svdf(::tflite::BuiltinOperator_SVDF, OperatorType::kSvdf));
  ops.emplace_back(new Transpose(::tflite::BuiltinOperator_TRANSPOSE,
                                 OperatorType::kTranspose));
  ops.emplace_back(
      new Mean(::tflite::BuiltinOperator_MEAN, OperatorType::kMean));

  // Custom Operators.
  ops.emplace_back(new Cast("CAST", OperatorType::kCast));
  ops.emplace_back(
      new DepthToSpace("DEPTH_TO_SPACE", OperatorType::kDepthToSpace));
  ops.emplace_back(new FakeQuant("FAKE_QUANT", OperatorType::kFakeQuant));
  ops.emplace_back(new Split("SPLIT", OperatorType::kTensorFlowSplit));
  ops.emplace_back(new TensorFlowUnsupported(
      "TENSORFLOW_UNSUPPORTED", OperatorType::kTensorFlowUnsupported));

  // There operators are supported by Toco, but not by TF Lite, and has no
  // attributes.
  ops.emplace_back(new SimpleOperator<NegOperator>("NEG", OperatorType::kNeg));
  ops.emplace_back(new SimpleOperator<TensorFlowRsqrtOperator>(
      "RSQRT", OperatorType::kTensorFlowRsqrt));
  ops.emplace_back(
      new SimpleOperator<TensorFlowRsqrtOperator>("DIV", OperatorType::kDiv));

  // Simple Operators.
  ops.emplace_back(new SimpleOperator<DequantizeOperator>(
      "DEQUANTIZE", OperatorType::kDequantize));
  ops.emplace_back(
      new SimpleOperator<FloorOperator>("FLOOR", OperatorType::kFloor));
  ops.emplace_back(
      new SimpleOperator<ReluOperator>("RELU", OperatorType::kRelu));
  ops.emplace_back(
      new SimpleOperator<Relu1Operator>("RELU_N1_TO_1", OperatorType::kRelu1));
  ops.emplace_back(
      new SimpleOperator<Relu6Operator>("RELU6", OperatorType::kRelu6));
  ops.emplace_back(new SimpleOperator<ResizeBilinearOperator>(
      "RESIZE_BILINEAR", OperatorType::kResizeBilinear));
  ops.emplace_back(new SimpleOperator<LogisticOperator>(
      "LOGISTIC", OperatorType::kLogistic));
  ops.emplace_back(
      new SimpleOperator<TanhOperator>("TANH", OperatorType::kTanh));

  return ops;
}
}  // namespace

std::map<OperatorType, std::unique_ptr<BaseOperator>> BuildOperatorByTypeMap() {
  std::map<OperatorType, std::unique_ptr<BaseOperator>> result;

  std::vector<std::unique_ptr<BaseOperator>> ops = BuildOperatorList();
  for (auto& op : ops) {
    result[op->type()] = std::move(op);
  }

  return result;
}

std::map<string, std::unique_ptr<BaseOperator>> BuildOperatorByNameMap() {
  std::map<string, std::unique_ptr<BaseOperator>> result;

  std::vector<std::unique_ptr<BaseOperator>> ops = BuildOperatorList();
  for (auto& op : ops) {
    result[op->name()] = std::move(op);
  }

  return result;
}

}  // namespace tflite

}  // namespace toco
