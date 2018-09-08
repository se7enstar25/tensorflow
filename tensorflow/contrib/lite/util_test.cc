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

#include <vector>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "tensorflow/contrib/lite/c/c_api_internal.h"
#include "tensorflow/contrib/lite/util.h"

namespace tflite {
namespace {

TEST(ConvertVectorToTfLiteIntArray, TestWithVector) {
  std::vector<int> input = {1, 2};
  TfLiteIntArray* output = ConvertVectorToTfLiteIntArray(input);
  ASSERT_NE(output, nullptr);
  EXPECT_EQ(output->size, 2);
  EXPECT_EQ(output->data[0], 1);
  EXPECT_EQ(output->data[1], 2);
  TfLiteIntArrayFree(output);
}

TEST(ConvertVectorToTfLiteIntArray, TestWithEmptyVector) {
  std::vector<int> input;
  TfLiteIntArray* output = ConvertVectorToTfLiteIntArray(input);
  ASSERT_NE(output, nullptr);
  EXPECT_EQ(output->size, 0);
  TfLiteIntArrayFree(output);
}

TEST(UtilTest, IsEagerOp) {
  EXPECT_TRUE(IsEagerOp("Eager"));
  EXPECT_TRUE(IsEagerOp("EagerOp"));
  EXPECT_FALSE(IsEagerOp("eager"));
  EXPECT_FALSE(IsEagerOp("Eage"));
  EXPECT_FALSE(IsEagerOp("OpEager"));
  EXPECT_FALSE(IsEagerOp(nullptr));
  EXPECT_FALSE(IsEagerOp(""));
}

}  // namespace
}  // namespace tflite

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
