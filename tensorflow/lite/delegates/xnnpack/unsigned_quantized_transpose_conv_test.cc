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

#include <cstdint>
#include <functional>
#include <memory>
#include <random>

#include <gtest/gtest.h>
#include "tensorflow/lite/delegates/xnnpack/quantized_transpose_conv_tester.h"
#include "tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h"

namespace tflite {
namespace xnnpack {

TEST(UnsignedQuantizedTransposeConvTest, 2x2Stride2) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(5, 25), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(2)
      .KernelWidth(2)
      .StrideHeight(2)
      .StrideWidth(2)
      .ValidPadding()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, 2x2Stride2NoBias) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(5, 25), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(2)
      .KernelWidth(2)
      .StrideHeight(2)
      .StrideWidth(2)
      .ValidPadding()
      .NoBias()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, 3x3Stride2) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(5, 25), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(3)
      .KernelWidth(3)
      .StrideHeight(2)
      .StrideWidth(2)
      .SamePadding()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, 3x3Stride2NoBias) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(5, 25), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(3)
      .KernelWidth(3)
      .StrideHeight(2)
      .StrideWidth(2)
      .SamePadding()
      .NoBias()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, 4x4Stride2) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(5, 25), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(4)
      .KernelWidth(4)
      .StrideHeight(2)
      .StrideWidth(2)
      .ValidPadding()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, 4x4Stride2NoBias) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(5, 25), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(4)
      .KernelWidth(4)
      .StrideHeight(2)
      .StrideWidth(2)
      .ValidPadding()
      .NoBias()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, 4x4Stride4) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(5, 25), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(4)
      .KernelWidth(4)
      .StrideHeight(4)
      .StrideWidth(4)
      .ValidPadding()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, 4x4Stride4NoBias) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(5, 25), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(4)
      .KernelWidth(4)
      .StrideHeight(4)
      .StrideWidth(4)
      .ValidPadding()
      .NoBias()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, SmallKernelWithSamePadding) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 7), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .SamePadding()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, SmallKernelWithSamePaddingNoBias) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 7), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .SamePadding()
      .NoBias()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, SmallKernelWithValidPadding) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 7), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .ValidPadding()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, SmallKernelWithValidPaddingNoBias) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 7), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .ValidPadding()
      .NoBias()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, StrideWithSamePadding) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(3, 5), std::ref(rng));
  auto stride_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 3), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .StrideHeight(stride_rng())
      .StrideWidth(stride_rng())
      .SamePadding()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, StrideWithSamePaddingNoBias) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(3, 5), std::ref(rng));
  auto stride_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 3), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .StrideHeight(stride_rng())
      .StrideWidth(stride_rng())
      .SamePadding()
      .NoBias()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, StrideWithValidPadding) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(3, 5), std::ref(rng));
  auto stride_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 3), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .StrideHeight(stride_rng())
      .StrideWidth(stride_rng())
      .ValidPadding()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, StrideWithValidPaddingNoBias) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(3, 5), std::ref(rng));
  auto stride_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 3), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .StrideHeight(stride_rng())
      .StrideWidth(stride_rng())
      .ValidPadding()
      .NoBias()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, SparseWeights) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(3, 5), std::ref(rng));
  auto stride_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 3), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .StrideHeight(stride_rng())
      .StrideWidth(stride_rng())
      .SamePadding()
      .SparseWeights()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, SparseWeightsNoBias) {
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(nullptr),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(3, 5), std::ref(rng));
  auto stride_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 3), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .StrideHeight(stride_rng())
      .StrideWidth(stride_rng())
      .SamePadding()
      .NoBias()
      .SparseWeights()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, MultiThreading) {
  TfLiteXNNPackDelegateOptions delegate_options =
      TfLiteXNNPackDelegateOptionsDefault();
  delegate_options.num_threads = 2;
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(&delegate_options),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(3, 5), std::ref(rng));
  auto stride_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 3), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .StrideHeight(stride_rng())
      .StrideWidth(stride_rng())
      .SamePadding()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, MultiThreadingNoBias) {
  TfLiteXNNPackDelegateOptions delegate_options =
      TfLiteXNNPackDelegateOptionsDefault();
  delegate_options.num_threads = 2;
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(&delegate_options),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(3, 5), std::ref(rng));
  auto stride_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 3), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .StrideHeight(stride_rng())
      .StrideWidth(stride_rng())
      .SamePadding()
      .NoBias()
      .Test(xnnpack_delegate.get());
}

TEST(UnsignedQuantizedTransposeConvTest, WeightsCache) {
  TfLiteXNNPackDelegateOptions delegate_options =
      TfLiteXNNPackDelegateOptionsDefault();
  std::unique_ptr<TfLiteXNNPackDelegateWeightsCache,
                  decltype(&TfLiteXNNPackDelegateWeightsCacheDelete)>
      weights_cache(TfLiteXNNPackDelegateWeightsCacheCreate(),
                    TfLiteXNNPackDelegateWeightsCacheDelete);
  delegate_options.weights_cache = weights_cache.get();
  std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)>
      xnnpack_delegate(TfLiteXNNPackDelegateCreate(&delegate_options),
                       TfLiteXNNPackDelegateDelete);

  std::random_device random_device;
  auto rng = std::mt19937(random_device());
  auto batch_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 4), std::ref(rng));
  auto output_rng =
      std::bind(std::uniform_int_distribution<int32_t>(10, 25), std::ref(rng));
  auto kernel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(3, 5), std::ref(rng));
  auto stride_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 3), std::ref(rng));
  auto channel_rng =
      std::bind(std::uniform_int_distribution<int32_t>(2, 5), std::ref(rng));

  QuantizedTransposeConvTester()
      .Unsigned(true)
      .BatchSize(batch_rng())
      .OutputHeight(output_rng())
      .OutputWidth(output_rng())
      .InputChannels(channel_rng())
      .OutputChannels(channel_rng())
      .KernelHeight(kernel_rng())
      .KernelWidth(kernel_rng())
      .StrideHeight(stride_rng())
      .StrideWidth(stride_rng())
      .SamePadding()
      .WeightsCache(weights_cache.get())
      .Test(xnnpack_delegate.get());
}

}  // namespace xnnpack
}  // namespace tflite
