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

#include "tensorflow/lite/c/builtin_op_data.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/kernels/kernel_runner.h"
#include "tensorflow/lite/micro/test_helpers.h"
#include "tensorflow/lite/micro/testing/micro_test.h"

namespace tflite {
namespace testing {
namespace {

void TestRound(const int* input_dims_data, const float* input_data,
               const float* expected_output_data, float* output_data) {
  TfLiteIntArray* input_dims = IntArrayFromInts(input_dims_data);
  TfLiteIntArray* output_dims = IntArrayFromInts(input_dims_data);
  const int output_dims_count = ElementCount(*output_dims);
  constexpr int inputs_size = 1;
  constexpr int outputs_size = 1;
  constexpr int tensors_size = inputs_size + outputs_size;
  TfLiteTensor tensors[tensors_size] = {
      CreateTensor(input_data, input_dims),
      CreateTensor(output_data, output_dims),
  };

  int inputs_array_data[] = {1, 0};
  TfLiteIntArray* inputs_array = IntArrayFromInts(inputs_array_data);
  int outputs_array_data[] = {1, 1};
  TfLiteIntArray* outputs_array = IntArrayFromInts(outputs_array_data);

  const TfLiteRegistration registration = tflite::ops::micro::Register_ROUND();
  micro::KernelRunner runner(registration, tensors, tensors_size, inputs_array,
                             outputs_array, nullptr, micro_test::reporter);

  TF_LITE_MICRO_EXPECT_EQ(kTfLiteOk, runner.InitAndPrepare());
  TF_LITE_MICRO_EXPECT_EQ(kTfLiteOk, runner.Invoke());

  for (int i = 0; i < output_dims_count; ++i) {
    TF_LITE_MICRO_EXPECT_NEAR(expected_output_data[i], output_data[i], 1e-5f);
  }
}

}  // namespace
}  // namespace testing
}  // namespace tflite

TF_LITE_MICRO_TESTS_BEGIN

TF_LITE_MICRO_TEST(SingleDim) {
  const int input_dims[] = {1, 6};
  const float input_data[] = {8.5, 0.0, 3.5, 4.2, -3.5, -4.5};
  const float golden[] = {8, 0, 4, 4, -4, -4};
  float output_data[6];
  tflite::testing::TestRound(input_dims, input_data, golden, output_data);
}

TF_LITE_MICRO_TEST(MultiDims) {
  const int input_dims[] = {4, 2, 1, 1, 6};
  const float input_data[] = {0.0001,  8.0001,  0.9999,  9.9999, 0.5,  -0.0001,
                              -8.0001, -0.9999, -9.9999, -0.5,   -2.5, 1.5};
  const float golden[] = {0, 8, 1, 10, 0, 0, -8, -1, -10, -0, -2, 2};
  float output_data[12];
  tflite::testing::TestRound(input_dims, input_data, golden, output_data);
}

TF_LITE_MICRO_TESTS_END
