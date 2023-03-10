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

#include "tensorflow/lite/delegates/xnnpack/fully_connected_tester.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#include <gtest/gtest.h>
#include "fp16.h"  // from @FP16
#include "flatbuffers/flatbuffers.h"  // from @flatbuffers
#include "tensorflow/lite/core/model.h"
#include "tensorflow/lite/delegates/xnnpack/test_util.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/schema/schema_conversion_utils.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

namespace tflite {
namespace xnnpack {

std::vector<int32_t> FullyConnectedTester::OutputShape() const {
  EXPECT_NE(input_shape_.size(), 0);
  if (KeepDims()) {
    std::vector<int32_t> output_shape(input_shape_.cbegin(),
                                      input_shape_.cend() - 1);
    output_shape.push_back(OutputChannels());
    return output_shape;
  } else {
    EXPECT_EQ(InputSize() % InputChannels(), 0);
    return std::vector<int32_t>(
        {InputSize() / InputChannels(), OutputChannels()});
  }
}

void FullyConnectedTester::Test(TfLiteDelegate* delegate) const {
  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto input_rng =
      std::bind(std::uniform_real_distribution<float>(), std::ref(rng));

  std::vector<char> buffer = CreateTfLiteModel();
  const Model* model = GetModel(buffer.data());

  std::unique_ptr<Interpreter> delegate_interpreter;
  ASSERT_EQ(
      InterpreterBuilder(
          model,
          ::tflite::ops::builtin::BuiltinOpResolverWithoutDefaultDelegates())(
          &delegate_interpreter),
      kTfLiteOk);
  std::unique_ptr<Interpreter> default_interpreter;
  ASSERT_EQ(
      InterpreterBuilder(
          model,
          ::tflite::ops::builtin::BuiltinOpResolverWithoutDefaultDelegates())(
          &default_interpreter),
      kTfLiteOk);

  ASSERT_TRUE(delegate_interpreter);
  ASSERT_TRUE(default_interpreter);

  ASSERT_EQ(delegate_interpreter->inputs().size(), 1);
  ASSERT_EQ(default_interpreter->inputs().size(), 1);

  ASSERT_EQ(delegate_interpreter->outputs().size(), 1);
  ASSERT_EQ(default_interpreter->outputs().size(), 1);

  ASSERT_EQ(delegate_interpreter->AllocateTensors(), kTfLiteOk);
  ASSERT_EQ(default_interpreter->AllocateTensors(), kTfLiteOk);

  ASSERT_EQ(delegate_interpreter->ModifyGraphWithDelegate(delegate), kTfLiteOk);

  if (weights_cache_ != nullptr) {
    TfLiteXNNPackDelegateWeightsCacheFinalizeHard(weights_cache_);
  }

  float* default_input_data = default_interpreter->typed_input_tensor<float>(0);
  std::generate(default_input_data, default_input_data + InputSize(),
                std::ref(input_rng));

  float* delegate_input_data =
      delegate_interpreter->typed_input_tensor<float>(0);
  std::copy(default_input_data, default_input_data + InputSize(),
            delegate_input_data);

  ASSERT_EQ(default_interpreter->Invoke(), kTfLiteOk);
  ASSERT_EQ(delegate_interpreter->Invoke(), kTfLiteOk);

  float* default_output_data =
      default_interpreter->typed_output_tensor<float>(0);
  float* delegate_output_data =
      delegate_interpreter->typed_output_tensor<float>(0);

  for (size_t i = 0; i < ComputeSize(OutputShape()); i++) {
    ASSERT_NEAR(default_output_data[i], delegate_output_data[i],
                std::numeric_limits<float>::epsilon() *
                    std::max(std::abs(default_output_data[i]) * 10.0f, 1.0f));
  }
}

std::vector<char> FullyConnectedTester::CreateTfLiteModel() const {
  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto range_rng = std::bind(
      std::uniform_real_distribution<float>(-25.0f, 25.0f), std::ref(rng));

  flatbuffers::FlatBufferBuilder builder;
  std::vector<flatbuffers::Offset<OperatorCode>> operator_codes{
      {CreateOperatorCode(builder, BuiltinOperator_FULLY_CONNECTED)}};
  std::vector<flatbuffers::Offset<Operator>> operators;
  std::vector<flatbuffers::Offset<Buffer>> buffers{
      {CreateBuffer(builder, builder.CreateVector({}))}};

  const std::vector<int32_t> filter_shape = {OutputChannels(), InputChannels()};
  const std::vector<int32_t> bias_shape = {OutputChannels()};
  std::vector<float> filter_scales;
  std::vector<int64_t> filter_zero_points;
  int32_t filter_quantized_dimension = 0;
  if (FP16Weights()) {
    operator_codes.emplace_back(
        CreateOperatorCode(builder, BuiltinOperator_DEQUANTIZE));

    std::vector<uint16_t> filter_data(InputChannels() * OutputChannels());
    std::vector<uint16_t> bias_data(OutputChannels());

    for (int32_t oc = 0; oc < OutputChannels(); oc++) {
      // Use the same range of all-positive or all-negative values to generate
      // all filter & bias weights within the same channel, but different ranges
      // for different output channels. This ensures that no catastrophic
      // cancellation occur, but test covers both positive and negative inputs.
      const float range = range_rng();
      auto value_rng =
          std::bind(fp16_ieee_from_fp32_value,
                    std::bind(std::uniform_real_distribution<float>(
                                  std::min(range, 0.0f), std::max(range, 0.0f)),
                              std::ref(rng)));

      bias_data[oc] = value_rng();
      for (int32_t ic = 0; ic < InputChannels(); ic++) {
        filter_data[oc * InputChannels() + ic] = value_rng();
      }
    }

    buffers.emplace_back(CreateBuffer(
        builder, builder.CreateVector(
                     reinterpret_cast<const uint8_t*>(filter_data.data()),
                     sizeof(uint16_t) * filter_data.size())));
    if (HasBias()) {
      buffers.emplace_back(CreateBuffer(
          builder, builder.CreateVector(
                       reinterpret_cast<const uint8_t*>(bias_data.data()),
                       sizeof(uint16_t) * bias_data.size())));
    }

    const std::array<int32_t, 1> dequantize_filter_inputs{{0}};
    const std::array<int32_t, 1> dequantize_filter_outputs{
        {2 + static_cast<int32_t>(HasBias())}};
    operators.emplace_back(CreateOperator(
        builder, /*opcode_index=*/1,
        builder.CreateVector<int32_t>(dequantize_filter_inputs.data(),
                                      dequantize_filter_inputs.size()),
        builder.CreateVector<int32_t>(dequantize_filter_outputs.data(),
                                      dequantize_filter_outputs.size())));
    if (HasBias()) {
      const std::array<int32_t, 1> dequantize_bias_inputs{{1}};
      const std::array<int32_t, 1> dequantize_bias_outputs{{4}};
      operators.emplace_back(CreateOperator(
          builder, /*opcode_index=*/1,
          builder.CreateVector<int32_t>(dequantize_bias_inputs.data(),
                                        dequantize_bias_inputs.size()),
          builder.CreateVector<int32_t>(dequantize_bias_outputs.data(),
                                        dequantize_bias_outputs.size())));
    }
  } else {
    std::vector<float> filter_data(InputChannels() * OutputChannels());
    std::vector<float> bias_data(OutputChannels());

    for (int32_t oc = 0; oc < OutputChannels(); oc++) {
      // Use the same range of all-positive or all-negative values to generate
      // all filter & bias weights within the same channel, but different ranges
      // for different output channels. This ensures that no catastrophic
      // cancellation occur, but test covers both positive and negative inputs.
      const float range = range_rng();
      auto value_rng =
          std::bind(std::uniform_real_distribution<float>(
                        std::min(range, 0.0f), std::max(range, 0.0f)),
                    std::ref(rng));

      bias_data[oc] = value_rng();
      for (int32_t ic = 0; ic < InputChannels(); ic++) {
        filter_data[oc * InputChannels() + ic] = value_rng();
      }
    }

    if (INT8Weights() || INT8ChannelWiseWeights()) {
      std::vector<int8_t> quantized_filter_data(filter_data.size());
      if (INT8Weights()) {
        filter_scales.resize(1, GetInt8QuantizationScale(filter_data));
        filter_zero_points.resize(1, 0);
        std::transform(filter_data.begin(), filter_data.end(),
                       quantized_filter_data.begin(),
                       std::bind(QuantizeInt8, std::placeholders::_1, 0,
                                 filter_scales[0]));
      } else {
        filter_quantized_dimension =
            static_cast<int32_t>(filter_shape.size()) - 1;
        const int32_t num_scales = filter_shape[filter_quantized_dimension];
        filter_scales = GetInt8QuantizationScalePerChannel(
            filter_data.data(), filter_quantized_dimension, filter_shape);
        filter_zero_points.resize(num_scales, 0);
        QuantizeInt8PerChannel(filter_scales.data(), filter_zero_points.data(),
                               filter_quantized_dimension, filter_data.data(),
                               quantized_filter_data.data(), filter_shape);
      }
      buffers.emplace_back(CreateBuffer(
          builder,
          builder.CreateVector(
              reinterpret_cast<const uint8_t*>(quantized_filter_data.data()),
              sizeof(int8_t) * quantized_filter_data.size())));
      operator_codes.emplace_back(
          CreateOperatorCode(builder, BuiltinOperator_DEQUANTIZE));
      const std::array<int32_t, 1> dequantize_filter_inputs{{0}};
      const std::array<int32_t, 1> dequantize_filter_outputs{{2}};
      operators.emplace_back(CreateOperator(
          builder, /*opcode_index=*/1,
          builder.CreateVector<int32_t>(dequantize_filter_inputs.data(),
                                        dequantize_filter_inputs.size()),
          builder.CreateVector<int32_t>(dequantize_filter_outputs.data(),
                                        dequantize_filter_outputs.size())));
    } else {
      buffers.emplace_back(CreateBuffer(
          builder, builder.CreateVector(
                       reinterpret_cast<const uint8_t*>(filter_data.data()),
                       sizeof(float) * filter_data.size())));
    }

    // Bias is stored in FP32 even when filter is quantized to INT8
    if (HasBias()) {
      buffers.emplace_back(CreateBuffer(
          builder, builder.CreateVector(
                       reinterpret_cast<const uint8_t*>(bias_data.data()),
                       sizeof(float) * bias_data.size())));
    }
  }

  const std::vector<int32_t> output_shape = OutputShape();
  std::vector<flatbuffers::Offset<Tensor>> tensors;
  if (FP16Weights()) {
    tensors.emplace_back(CreateTensor(
        builder,
        builder.CreateVector<int32_t>(filter_shape.data(), filter_shape.size()),
        TensorType_FLOAT16, /*buffer=*/1));
    if (HasBias()) {
      tensors.emplace_back(CreateTensor(
          builder,
          builder.CreateVector<int32_t>(bias_shape.data(), bias_shape.size()),
          TensorType_FLOAT16, /*buffer=*/2));
    }
  } else if (INT8Weights() || INT8ChannelWiseWeights()) {
    tensors.emplace_back(CreateTensor(
        builder,
        builder.CreateVector<int32_t>(filter_shape.data(), filter_shape.size()),
        TensorType_INT8, /*buffer=*/1, /*name=*/0,
        CreateQuantizationParameters(
            builder, /*min=*/0, /*max=*/0,
            builder.CreateVector<float>(filter_scales),
            builder.CreateVector<int64_t>(filter_zero_points),
            /*details_type=*/QuantizationDetails_NONE,
            /*details=*/0, filter_quantized_dimension)));
  }
  tensors.emplace_back(CreateTensor(
      builder,
      builder.CreateVector<int32_t>(InputShape().data(), InputShape().size()),
      TensorType_FLOAT32));
  tensors.emplace_back(CreateTensor(
      builder,
      builder.CreateVector<int32_t>(filter_shape.data(), filter_shape.size()),
      TensorType_FLOAT32,
      /*buffer=*/
      (FP16Weights() || INT8Weights() || INT8ChannelWiseWeights()) ? 0 : 1));
  if (HasBias()) {
    tensors.emplace_back(CreateTensor(
        builder,
        builder.CreateVector<int32_t>(bias_shape.data(), bias_shape.size()),
        TensorType_FLOAT32, /*buffer=*/FP16Weights() ? 0 : 2));
  }
  tensors.emplace_back(CreateTensor(
      builder,
      builder.CreateVector<int32_t>(output_shape.data(), output_shape.size()),
      TensorType_FLOAT32));

  flatbuffers::Offset<FullyConnectedOptions> fully_connected_options =
      CreateFullyConnectedOptions(builder, Activation(),
                                  FullyConnectedOptionsWeightsFormat_DEFAULT,
                                  KeepDims());

  std::vector<int32_t> op_inputs{{static_cast<int32_t>(tensors.size()) - 3,
                                  static_cast<int32_t>(tensors.size()) - 2}};
  if (HasBias()) {
    op_inputs.insert(op_inputs.begin(),
                     static_cast<int32_t>(tensors.size()) - 4);
  }
  const std::array<int32_t, 1> op_outputs{
      {static_cast<int32_t>(tensors.size()) - 1}};
  operators.emplace_back(CreateOperator(
      builder, /*opcode_index=*/0,
      builder.CreateVector<int32_t>(op_inputs.data(), op_inputs.size()),
      builder.CreateVector<int32_t>(op_outputs.data(), op_outputs.size()),
      BuiltinOptions_FullyConnectedOptions, fully_connected_options.Union()));

  const std::array<int32_t, 1> subgraph_inputs{
      {static_cast<int>(tensors.size()) - 3 - static_cast<int32_t>(HasBias())}};
  const std::array<int32_t, 1> subgraph_outputs{
      {static_cast<int>(tensors.size()) - 1}};
  flatbuffers::Offset<SubGraph> subgraph = CreateSubGraph(
      builder, builder.CreateVector(tensors.data(), tensors.size()),
      builder.CreateVector<int32_t>(subgraph_inputs.data(),
                                    subgraph_inputs.size()),
      builder.CreateVector<int32_t>(subgraph_outputs.data(),
                                    subgraph_outputs.size()),
      builder.CreateVector(operators.data(), operators.size()));

  flatbuffers::Offset<flatbuffers::String> description =
      builder.CreateString("Fully Connected model");

  flatbuffers::Offset<Model> model_buffer = CreateModel(
      builder, TFLITE_SCHEMA_VERSION,
      builder.CreateVector(operator_codes.data(), operator_codes.size()),
      builder.CreateVector(&subgraph, 1), description,
      builder.CreateVector(buffers.data(), buffers.size()));

  builder.Finish(model_buffer);

  return std::vector<char>(builder.GetBufferPointer(),
                           builder.GetBufferPointer() + builder.GetSize());
}

int32_t FullyConnectedTester::ComputeSize(const std::vector<int32_t>& shape) {
  return std::accumulate(shape.cbegin(), shape.cend(), 1,
                         std::multiplies<int32_t>());
}

}  // namespace xnnpack
}  // namespace tflite
