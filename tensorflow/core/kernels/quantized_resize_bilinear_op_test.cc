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

#define EIGEN_USE_THREADS

#include <vector>

#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/ops/array_ops.h"
#include "tensorflow/cc/ops/const_op.h"
#include "tensorflow/cc/ops/image_ops.h"
#include "tensorflow/core/common_runtime/gradients.h"
#include "tensorflow/core/framework/node_def_builder.h"
#include "tensorflow/core/framework/node_def_util.h"
#include "tensorflow/core/framework/shape_inference_testutil.h"
#include "tensorflow/core/framework/tensor_testutil.h"
#include "tensorflow/core/kernels/quantization_utils.h"
#include "tensorflow/core/lib/core/status_test_util.h"
#include "tensorflow/core/platform/test.h"

namespace tensorflow {

namespace {
constexpr const float RESIZE_VAL_TOLERANCE = 1.0e-8;

template <typename T>
Tensor BuildTensor(const int batch_size, const int height, const int width,
                   const int channels, const float ratio, const float min,
                   const float max) {
  Tensor tensor(DataTypeToEnum<T>::value,
                TensorShape({batch_size, height, width, channels}));
  for (int64_t i = 0; i < tensor.NumElements(); ++i) {
    tensor.flat<T>()(i) =
        FloatToQuantized<T>(static_cast<float>(i) / ratio, min, max);
  }
  return tensor;
}

template <>
Tensor BuildTensor<float>(const int batch_size, const int height,
                          const int width, const int channels,
                          const float ratio, const float min, const float max) {
  Tensor tensor(DT_FLOAT, TensorShape({batch_size, height, width, channels}));
  for (int64_t i = 0; i < tensor.NumElements(); ++i) {
    tensor.flat<float>()(i) = static_cast<float>(i) / ratio;
  }
  return tensor;
}

float CalculateResizeScale(int64_t in_size, int64_t out_size,
                           bool align_corners) {
  return (align_corners && out_size > 1)
             ? (in_size - 1) / static_cast<float>(out_size - 1)
             : in_size / static_cast<float>(out_size);
}

inline std::tuple<int64_t, int64_t, float> GetReferenceWeight(
    const bool half_pixel_centers, const int64_t out_size,
    const int64_t in_size, const int step, const int index, const float scale) {
  const float in = half_pixel_centers
                       ? (static_cast<float>(index) + 0.5f) * scale - 0.5f
                       : index * scale;
  const float in_f = std::floor(in);
  const int64_t lower =
      std::max(static_cast<int64_t>(in_f), static_cast<int64_t>(0));
  const int64_t upper =
      std::min(static_cast<int64_t>(std::ceil(in)), in_size - 1);
  return std::make_tuple(lower * step, upper * step, in - in_f);
}

template <typename T>
T ComputeLerpReference(const T in_top_left, const T in_top_right,
                       const T in_bottom_left, const T in_bottom_right,
                       const float x_lerp, const float y_lerp, const float min,
                       const float max) {
  const float top_left = QuantizedToFloat<T>(in_top_left, min, max);
  const float top_right = QuantizedToFloat<T>(in_top_right, min, max);
  const float bottom_left = QuantizedToFloat<T>(in_bottom_left, min, max);
  const float bottom_right = QuantizedToFloat<T>(in_bottom_right, min, max);
  const float top = top_left + (top_right - top_left) * x_lerp;
  const float bottom = bottom_left + (bottom_right - bottom_left) * x_lerp;
  const float out = top + (bottom - top) * y_lerp;
  return FloatToQuantized<T>(out, min, max);
}

template <>
float ComputeLerpReference<float>(const float in_top_left,
                                  const float in_top_right,
                                  const float in_bottom_left,
                                  const float in_bottom_right,
                                  const float x_lerp, const float y_lerp,
                                  const float min, const float max) {
  const float top = in_top_left + (in_top_right - in_top_left) * x_lerp;
  const float bottom =
      in_bottom_left + (in_bottom_right - in_bottom_left) * x_lerp;
  return top + (bottom - top) * y_lerp;
}

template <typename T>
T CalcReferenceResizedVal(const T* image_data, const bool half_pixel_centers,
                          const int batch_size, const int64_t in_height,
                          const int64_t in_width, const int64_t out_height,
                          const int64_t out_width, const int channels,
                          const float height_scale, const float width_scale,
                          const float min, const float max, const int b,
                          const int64_t x, const int64_t y, const int c) {
  const std::tuple<int64_t, int64_t, float> x_weight = GetReferenceWeight(
      half_pixel_centers, out_width, in_width, channels, x, width_scale);
  const std::tuple<int64_t, int64_t, float> y_weight = GetReferenceWeight(
      half_pixel_centers, out_height, in_height, 1, y, height_scale);

  const int64_t in_row_size = in_width * channels;
  const int64_t in_batch_num_values = in_height * in_row_size;

  const int y_lower_index =
      b * in_batch_num_values + std::get<0>(y_weight) * in_row_size;
  const int y_upper_index =
      b * in_batch_num_values + std::get<1>(y_weight) * in_row_size;

  const int64_t xs_lower = std::get<0>(x_weight);
  const int64_t xs_upper = std::get<1>(x_weight);
  const float xs_lerp = std::get<2>(x_weight);
  const float ys_lerp = std::get<2>(y_weight);
  const float top_left = image_data[y_lower_index + xs_lower + c];
  const float top_right = image_data[y_lower_index + xs_upper + c];
  const float bottom_left = image_data[y_upper_index + xs_lower + c];
  const float bottom_right = image_data[y_upper_index + xs_upper + c];
  const float val =
      ComputeLerpReference<T>(top_left, top_right, bottom_left, bottom_right,
                              xs_lerp, ys_lerp, min, max);
  return val;
}

template <typename T>
void CheckTensorValue(const T* in_data, const T* out_data, const int batch_size,
                      const int64_t in_height, const int64_t in_width,
                      const int64_t out_height, const int64_t out_width,
                      const int channels, const bool align_corners,
                      const bool half_pixel_centers, const float min,
                      const float max, const float tolerance,
                      const bool relative) {
  const int64_t out_row_size = out_width * channels;
  const float height_scale =
      CalculateResizeScale(in_height, out_height, align_corners);
  const float width_scale =
      CalculateResizeScale(in_width, out_width, align_corners);

  for (int b = 0; b < batch_size; ++b) {
    for (int64_t y = 0; y < out_height; ++y) {
      for (int64_t x = 0; x < out_width; ++x) {
        for (int c = 0; c < channels; ++c) {
          const T ref_qval = CalcReferenceResizedVal<T>(
              in_data, half_pixel_centers, batch_size, in_height, in_width,
              out_height, out_width, channels, height_scale, width_scale, min,
              max, b, x, y, c);
          const T qval =
              out_data[(b * out_height + y) * out_row_size + x * channels + c];
          const float ref_val = QuantizedToFloat<T>(ref_qval, min, max);
          const float val = QuantizedToFloat<T>(qval, min, max);
          if (!relative) {
            const int q_tolerance = std::round(tolerance);
            EXPECT_TRUE(std::abs(static_cast<int32>(ref_qval) -
                                 static_cast<int32>(qval)) <= q_tolerance)
                << "ref = " << ref_val << ", val = " << val << ", " << b << ", "
                << y << ", " << x << ", " << c << ", qval = " << qval
                << ", ref qval = " << ref_qval << ", " << q_tolerance;
          } else {
            const float rel_tolerance = std::max(ref_val, 1.0f) * tolerance;
            EXPECT_NEAR(ref_val, val, rel_tolerance)
                << "ref = " << ref_val << ", val = " << val << ", " << b << ", "
                << y << ", " << x << ", " << c << ", ref qval = " << qval;
          }
        }
      }
    }
  }
}

void TestResizeBilinear(const Tensor& image_tensor, const DataType dt,
                        const Input::Initializer& new_size,
                        const bool show_time, const int64_t iterations,
                        const float min, const float max,
                        const bool half_pixel_centers,
                        std::vector<Tensor>* outputs) {
  Scope root = Scope::NewRootScope();

  Output placeholder = ops::Placeholder(root.WithOpName("placeholder"), dt);
  Output size = ops::Const<int32>(root.WithOpName("size"), new_size);
  Output in_min = ops::Const<float>(root.WithOpName("min"), min);
  Output in_max = ops::Const<float>(root.WithOpName("max"), max);

  ops::QuantizedResizeBilinear qrb = ops::QuantizedResizeBilinear(
      root.WithOpName("qrb"), placeholder, size, in_min, in_max,
      ops::QuantizedResizeBilinear::HalfPixelCenters(half_pixel_centers));

  TF_EXPECT_OK(root.status());

  ClientSession session(root);

  int64_t total_duration = 0;
  outputs->clear();

  for (int i = 0; i < iterations; ++i) {
    const int64_t start_time = Env::Default()->NowMicros();
    TF_EXPECT_OK(session.Run({{placeholder, image_tensor}},
                             {qrb.resized_images, qrb.out_min, qrb.out_max},
                             outputs));
    const int64_t end_time = Env::Default()->NowMicros();
    total_duration += end_time - start_time;
  }
  const int64_t one_run_duration = total_duration / iterations;

  const int64_t num_ops = outputs->at(0).NumElements();

  const double million_ops_per_second =
      (iterations * num_ops) / static_cast<double>(total_duration);

  if (show_time) {
    LOG(INFO) << "Time resize bilinear: "
              << TensorShape(image_tensor.shape()).DebugString()
              << ": iterations=" << iterations
              << ", MOps/s=" << million_ops_per_second
              << ", one_run_duration=" << one_run_duration
              << ", total_duration=" << total_duration;
  }
}

}  // namespace

void TestResizeBilinearOneDim() {
  constexpr float TOLERANCE = 1.0e-5;
  constexpr int IN_WIDTH = 128;
  constexpr int OUT_WIDTH = 256;
  constexpr float MIN = 0.0f;
  constexpr float MAX = 256.0f;
  constexpr float SCALE = static_cast<float>(IN_WIDTH) / OUT_WIDTH;
  Tensor image_quantized_tensor(DT_QINT32, TensorShape({1, 1, IN_WIDTH, 1}));

  for (int64_t i = 0; i < image_quantized_tensor.NumElements(); ++i) {
    image_quantized_tensor.flat<qint32>()(i) =
        FloatToQuantized<qint32>(static_cast<float>(i), MIN, MAX);
  }

  std::vector<Tensor> outputs;
  TestResizeBilinear(image_quantized_tensor, DT_QINT32, {1, OUT_WIDTH}, false,
                     1, MIN, MAX, false, &outputs);
  ASSERT_EQ(3, outputs.size());
  ASSERT_EQ(OUT_WIDTH, outputs.at(0).NumElements());
  ASSERT_EQ(4, outputs.at(0).shape().dims());
  ASSERT_EQ(OUT_WIDTH, outputs.at(0).shape().dim_size(2));

  // Manual value testing
  for (int64_t i = 0; i < outputs.at(0).NumElements(); ++i) {
    const float resized_image_val =
        QuantizedToFloat<qint32>(outputs.at(0).flat<qint32>()(i), MIN, MAX);
    float expected_val = 0.0f;
    if (i == 0 || i == outputs.at(0).NumElements() - 1 || i % 2 == 0) {
      expected_val = QuantizedToFloat<qint32>(
          image_quantized_tensor.flat<qint32>()(i / 2), MIN, MAX);
    } else {
      const float image_val0 = QuantizedToFloat<qint32>(
          image_quantized_tensor.flat<qint32>()(i / 2), MIN, MAX);
      const float image_val1 = QuantizedToFloat<qint32>(
          image_quantized_tensor.flat<qint32>()(i / 2 + 1), MIN, MAX);
      expected_val = (image_val0 + image_val1) * SCALE;
    }
    VLOG(1) << "(" << i << ") " << expected_val << ", " << resized_image_val;
    EXPECT_NEAR(expected_val, resized_image_val, RESIZE_VAL_TOLERANCE)
        << expected_val << ", " << resized_image_val;
  }

  // Value testing with reference implementation
  CheckTensorValue<qint32>(image_quantized_tensor.flat<qint32>().data(),
                           outputs.at(0).flat<qint32>().data(),
                           /*batch_size=*/1,
                           /*in_height=*/IN_WIDTH,
                           /*in_width=*/1,
                           /*out_height=*/OUT_WIDTH,
                           /*out_width=*/1,
                           /*channels=*/1,
                           /*align_corners=*/false,
                           /*half_pixel_centers=*/false, MIN, MAX, TOLERANCE,
                           true);
}

template <typename T>
void RunTestResizeBilinearTwoDims(int batch_size, int in_height, int in_width,
                                  int out_height, int out_width, int channels,
                                  float tolerance, bool relative,
                                  const bool half_pixel_centers) {
  constexpr float RATIO = 100.0f;
  const float min = 0.0f;
  const float max = batch_size * in_height * in_width * channels / RATIO;

  const Tensor image_quantized_tensor = BuildTensor<T>(
      batch_size, in_height, in_width, channels, RATIO, min, max);

  std::vector<Tensor> outputs;
  TestResizeBilinear(image_quantized_tensor, DataTypeToEnum<T>::value,
                     {out_height, out_width}, false, 1, min, max,
                     half_pixel_centers, &outputs);
  CheckTensorValue<T>(
      image_quantized_tensor.flat<T>().data(), outputs.at(0).flat<T>().data(),
      batch_size, in_height, in_width, out_height, out_width, channels,
      /*align_corners=*/false,
      /*half_pixel_centers=*/half_pixel_centers, min, max, tolerance, relative);
}

template <typename T>
void RunBenchmarkResizeBilinearTwoDims(int batch_size, int in_height,
                                       int in_width, int out_height,
                                       int out_width, int channels,
                                       int iteration,
                                       const bool half_pixel_centers) {
  constexpr float RATIO = 100.0f;
  const float min = 0.0f;
  const float max = batch_size * in_height * in_width * channels / RATIO;

  const Tensor image_quantized_tensor = BuildTensor<T>(
      batch_size, in_height, in_width, channels, RATIO, min, max);

  std::vector<Tensor> outputs;
  TestResizeBilinear(image_quantized_tensor, DataTypeToEnum<T>::value,
                     {out_height, out_width}, true, iteration, min, max, false,
                     &outputs);
}

template <typename T>
void TestResizeBilinearTwoDimsType(const float tolerance, const bool relative,
                                   const bool half_pixel_centers) {
  RunTestResizeBilinearTwoDims<T>(1, 1, 1, 1, 1, 1, tolerance, relative,
                                  half_pixel_centers);
  RunTestResizeBilinearTwoDims<T>(1, 1, 128, 1, 256, 1, tolerance, relative,
                                  half_pixel_centers);
  RunTestResizeBilinearTwoDims<T>(1, 128, 1, 256, 1, 1, tolerance, relative,
                                  half_pixel_centers);
  RunTestResizeBilinearTwoDims<T>(1, 128, 128, 256, 256, 1, tolerance, relative,
                                  half_pixel_centers);
  RunTestResizeBilinearTwoDims<T>(1, 256, 256, 128, 128, 1, tolerance, relative,
                                  half_pixel_centers);
  RunTestResizeBilinearTwoDims<T>(1, 1, 128, 1, 256, 2, tolerance, relative,
                                  half_pixel_centers);
  RunTestResizeBilinearTwoDims<T>(1, 128, 1, 256, 1, 2, tolerance, relative,
                                  half_pixel_centers);
  RunTestResizeBilinearTwoDims<T>(1, 128, 128, 256, 256, 2, tolerance, relative,
                                  half_pixel_centers);
  RunTestResizeBilinearTwoDims<T>(1, 256, 256, 128, 128, 2, tolerance, relative,
                                  half_pixel_centers);
  RunTestResizeBilinearTwoDims<T>(1, 1, 16, 1, 32, 3, tolerance, relative,
                                  half_pixel_centers);
  RunTestResizeBilinearTwoDims<T>(1, 1, 128, 1, 256, 3, tolerance, relative,
                                  half_pixel_centers);
  RunTestResizeBilinearTwoDims<T>(1, 128, 128, 256, 256, 3, tolerance, relative,
                                  half_pixel_centers);
  RunTestResizeBilinearTwoDims<T>(1, 256, 256, 128, 128, 3, tolerance, relative,
                                  half_pixel_centers);
}

void TestResizeBilinearTwoDims() {
  for (const bool half_pixel_centers : {false, true}) {
    TestResizeBilinearTwoDimsType<quint8>(1.0f, false, half_pixel_centers);
    TestResizeBilinearTwoDimsType<qint32>(1.0e-5, true, half_pixel_centers);
    TestResizeBilinearTwoDimsType<float>(1.0e-5, true, half_pixel_centers);
  }
}

template <typename T>
void RunBenchmarkResizeBilinearTwoDimsType() {
  constexpr int ITER = 100;
  RunBenchmarkResizeBilinearTwoDims<T>(1, 1, 1, 2, 2, 1, ITER, false);
  RunBenchmarkResizeBilinearTwoDims<T>(1, 128, 128, 256, 256, 1, ITER, false);
  RunBenchmarkResizeBilinearTwoDims<T>(1, 128, 128, 256, 256, 3, ITER, false);
  RunBenchmarkResizeBilinearTwoDims<T>(1, 64, 64, 128, 128, 2, ITER, false);
  RunBenchmarkResizeBilinearTwoDims<T>(1, 32, 32, 64, 64, 16, ITER, false);
}

void RunBenchmarkResizeBilinearTwoDims() {
  LOG(INFO) << "Benchmark quint8";
  RunBenchmarkResizeBilinearTwoDimsType<quint8>();
  LOG(INFO) << "Benchmark qint32";
  RunBenchmarkResizeBilinearTwoDimsType<qint32>();
  LOG(INFO) << "Benchmark float";
  RunBenchmarkResizeBilinearTwoDimsType<float>();
}

}  // namespace tensorflow

#define RUN_TEST(t) \
  TEST(QuantizationResizeBilinearTest, t) { tensorflow::t(); }

RUN_TEST(TestResizeBilinearOneDim);
RUN_TEST(TestResizeBilinearTwoDims);

#if defined(__ANDROID__)

RUN_TEST(RunBenchmarkResizeBilinearTwoDims);

#endif  // __ANDROID__

int main(int argc, char** argv) {
  // On Linux, add: absl::SetFlag(&FLAGS_logtostderr, true);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
