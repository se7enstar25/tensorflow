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
#include "tensorflow/lite/tools/versioning/op_signature.h"

#include <cstdlib>

#include "tensorflow/lite/core/api/flatbuffer_conversions.h"
#include "tensorflow/lite/kernels/internal/compatibility.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/schema/schema_utils.h"
#include "tensorflow/lite/stderr_reporter.h"

namespace tflite {
namespace {

// A BuiltinDataAllocator which just uses malloc()/free().
class MallocDataAllocator : public BuiltinDataAllocator {
 public:
  void* Allocate(size_t size, size_t alignment_hint) override {
    return malloc(size);
  }
  void Deallocate(void* data) override { free(data); }
};

// Get the number of dimensions of a tensor with idx of an operator op.
inline int GetNumDims(const SubGraph* subgraph, const Operator* op, int idx) {
  return subgraph->tensors()->Get(op->inputs()->Get(idx))->shape()->size();
}

std::vector<OpSignatureTensorSpec> GetOpSignatureTensorSpecs(
    const flatbuffers::Vector<int32_t>* tensors, const SubGraph* subgraph) {
  std::vector<OpSignatureTensorSpec> tensor_specs;
  StderrReporter error_reporter;

  for (int32_t i = 0; i < tensors->Length(); ++i) {
    int32_t tensor_no = tensors->Get(i);

    OpSignatureTensorSpec tensor_spec = {kTfLiteNoType};
    if (tensor_no >= 0) {
      if (subgraph->tensors() && tensor_no < subgraph->tensors()->Length()) {
        ConvertTensorType(subgraph->tensors()->Get(tensor_no)->type(),
                          &tensor_spec.type, &error_reporter);
        const flatbuffers::Vector<int32_t>* shape_vec =
            subgraph->tensors()->Get(tensor_no)->shape();
        if (shape_vec) {
          for (int32_t j = 0; j < shape_vec->Length(); ++j) {
            tensor_spec.dims.push_back(shape_vec->Get(j));
          }
        }
      }
    }
    tensor_specs.push_back(tensor_spec);
  }
  return tensor_specs;
}

std::vector<OpSignatureTensorSpec> GetOpSignatureTensorSpecs(
    TfLiteIntArray* tensors, const TfLiteContext* context,
    const TfLiteNode* tflite_node) {
  std::vector<OpSignatureTensorSpec> tensor_specs;

  for (int32_t i = 0; i < tensors->size; ++i) {
    int32_t tensor_no = tensors->data[i];

    OpSignatureTensorSpec tensor_spec = {kTfLiteNoType};
    if (tensor_no >= 0) {
      const TfLiteTensor* tfl_tensor =
          GetOptionalInputTensor(context, tflite_node, tensor_no);
      if (tfl_tensor != nullptr) {
        tensor_spec.type = tfl_tensor->type;
        tensor_spec.is_const = (tfl_tensor->allocation_type == kTfLiteMmapRo);
        if (tfl_tensor->dims_signature) {
          // To handle dynamic shapes. Unknown dimensions are represented as -1.
          for (int32_t j = 0; j < tfl_tensor->dims_signature->size; ++j) {
            tensor_spec.dims.push_back(tfl_tensor->dims_signature->data[j]);
          }
        } else if (tfl_tensor->dims) {
          for (int32_t j = 0; i < tfl_tensor->dims->size; ++j) {
            tensor_spec.dims.push_back(tfl_tensor->dims->data[j]);
          }
        }
      }
    }
    tensor_specs.push_back(tensor_spec);
  }
  return tensor_specs;
}

}  // namespace

OpSignature GetOpSignature(const OperatorCode* op_code, const Operator* op,
                           const SubGraph* subgraph) {
  auto builtin_code = GetBuiltinCode(op_code);
  OpSignature op_sig = {builtin_code};
  std::memset(&op_sig.ext_options, 0, sizeof(op_sig.ext_options));

  if (builtin_code != BuiltinOperator_CUSTOM) {
    StderrReporter error_reporter;
    MallocDataAllocator allocator;
    ParseOpData(op, builtin_code, &error_reporter, &allocator,
                &op_sig.builtin_data);
  }

  switch (builtin_code) {
    case BuiltinOperator_DEPTHWISE_CONV_2D: {
      const Tensor* filter_tensor =
          subgraph->tensors()->Get(op->inputs()->Get(1));
      const QuantizationParameters* filter_quant =
          filter_tensor->quantization();
      int num_channels = filter_tensor->shape()->Get(3);
      if (filter_quant && filter_quant->scale() &&
          filter_quant->scale()->Length() &&
          filter_quant->scale()->Length() == num_channels) {
        op_sig.ext_options.depthwise_conv_2d.is_per_channel_quantized = true;
      }
    } break;

    case BuiltinOperator_FULLY_CONNECTED: {
      const Tensor* weight_tensor =
          subgraph->tensors()->Get(op->inputs()->Get(1));
      op_sig.ext_options.fully_connected.sparse_weight =
          (weight_tensor->sparsity() != nullptr);
    } break;

    case BuiltinOperator_MUL: {
      if (op->inputs()->Length() < 2 || op->outputs()->Length() < 1) {
        break;
      }
      const Tensor* input1_tensor =
          subgraph->tensors()->Get(op->inputs()->Get(0));
      const Tensor* input2_tensor =
          subgraph->tensors()->Get(op->inputs()->Get(1));
      const Tensor* output_tensor =
          subgraph->tensors()->Get(op->outputs()->Get(0));
      const QuantizationParameters* input1_quant =
          input1_tensor->quantization();
      const QuantizationParameters* input2_qunt = input2_tensor->quantization();
      const QuantizationParameters* output_quant =
          output_tensor->quantization();
      if (input1_quant && input1_quant->scale() &&
          input1_quant->scale()->Length() && input2_qunt &&
          input2_qunt->scale() && input2_qunt->scale()->Length() &&
          output_quant && output_quant->scale() &&
          output_quant->scale()->Length()) {
        op_sig.ext_options.mul.input1_scale = input1_quant->scale()->Get(0);
        op_sig.ext_options.mul.input2_scale = input2_qunt->scale()->Get(0);
        op_sig.ext_options.mul.output_scale = output_quant->scale()->Get(0);
      }
    } break;

    case BuiltinOperator_CONV_2D: {
      const Tensor* filter_tensor =
          subgraph->tensors()->Get(op->inputs()->Get(1));
      const QuantizationParameters* filter_quant =
          filter_tensor->quantization();
      int num_channels = filter_tensor->shape()->Get(0);
      if (filter_quant && filter_quant->scale() &&
          filter_quant->scale()->Length() &&
          filter_quant->scale()->Length() == num_channels) {
        op_sig.ext_options.conv_2d.is_per_channel_quantized = true;
      }
    } break;

    case BuiltinOperator_STRIDED_SLICE: {
      op_sig.ext_options.strided_slice.num_dims = GetNumDims(subgraph, op, 0);
    } break;

    case BuiltinOperator_ABS: {
      if (subgraph->tensors()->Get(op->inputs()->Get(0))->quantization()) {
        op_sig.ext_options.abs.input_quantized = true;
      }
    } break;

    default:
      break;
  }

  op_sig.inputs = GetOpSignatureTensorSpecs(op->inputs(), subgraph);
  op_sig.outputs = GetOpSignatureTensorSpecs(op->outputs(), subgraph);
  return op_sig;
}

OpSignature GetOpSignature(const TfLiteContext* context,
                           const TfLiteNode* tflite_node,
                           const TfLiteRegistration* registration) {
  OpSignature op_sig = {
      static_cast<BuiltinOperator>(registration->builtin_code)};
  op_sig.builtin_data = tflite_node->builtin_data;
  std::memset(&op_sig.ext_options, 0, sizeof(op_sig.ext_options));

  op_sig.inputs =
      GetOpSignatureTensorSpecs(tflite_node->inputs, context, tflite_node);
  op_sig.outputs =
      GetOpSignatureTensorSpecs(tflite_node->outputs, context, tflite_node);
  return op_sig;
}

}  // namespace tflite
