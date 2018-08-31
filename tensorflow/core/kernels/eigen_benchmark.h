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

#ifndef TENSORFLOW_CORE_KERNELS_EIGEN_BENCHMARK_H_
#define TENSORFLOW_CORE_KERNELS_EIGEN_BENCHMARK_H_

#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"
#include "tensorflow/core/framework/tensor_types.h"
#include "tensorflow/core/kernels/eigen_backward_spatial_convolutions.h"
#include "tensorflow/core/kernels/eigen_spatial_convolutions.h"
#include "tensorflow/core/platform/test_benchmark.h"

using ::tensorflow::TTypes;

template <typename Scalar, typename Device>
class SpatialConvolutionBenchmarksSuite {
 public:
  using Input = TTypes<float, 4>::ConstTensor;
  using Filter = TTypes<float, 4>::ConstTensor;
  using Output = TTypes<float, 4>::Tensor;

  using Dimensions = Eigen::DSizes<Eigen::Index, 4>;

  SpatialConvolutionBenchmarksSuite(int iters, Device& device)
      : iters_(iters), device_(device) {}

  Eigen::Index BufferSize(const Dimensions& dims) {
    return dims.TotalSize() * sizeof(Scalar);
  }

  void SpatialConvolution(Dimensions input_dims, Dimensions filter_dims) {
    Dimensions output_dims(input_dims[0],    // batch
                           input_dims[1],    // input_height
                           input_dims[2],    // input_width
                           filter_dims[3]);  // filter_count

    Scalar* input_data =
        static_cast<Scalar*>(device_.allocate(BufferSize(input_dims)));
    Scalar* filter_data =
        static_cast<Scalar*>(device_.allocate(BufferSize(filter_dims)));
    Scalar* output_data =
        static_cast<Scalar*>(device_.allocate(BufferSize(output_dims)));

    device_.memset(input_data, 123, BufferSize(input_dims));
    device_.memset(filter_data, 123, BufferSize(filter_dims));

    Input input(input_data, input_dims);
    Filter filter(filter_data, filter_dims);
    Output output(output_data, output_dims);

    ::tensorflow::testing::StartTiming();
    for (int i = 0; i < iters_; ++i) {
      output.device(device_) = Eigen::SpatialConvolution(input, filter);
      tensorflow::testing::DoNotOptimize(output);
    }
    ::tensorflow::testing::StopTiming();

    device_.deallocate(input_data);
    device_.deallocate(filter_data);
    device_.deallocate(output_data);
  }

  void SpatialConvolutionBackwardInput(Dimensions input_dims,
                                       Dimensions filter_dims) {
    Dimensions output_dims(input_dims[0],    // batch
                           input_dims[1],    // input_height
                           input_dims[2],    // input_width
                           filter_dims[3]);  // filter_count

    // Assuming that the convolution had SAME padding.
    Eigen::Index input_rows = input_dims[1];
    Eigen::Index input_cols = input_dims[2];

    Scalar* input_data =
        static_cast<Scalar*>(device_.allocate(BufferSize(input_dims)));
    Scalar* filter_data =
        static_cast<Scalar*>(device_.allocate(BufferSize(filter_dims)));
    Scalar* output_data =
        static_cast<Scalar*>(device_.allocate(BufferSize(output_dims)));

    device_.memset(input_data, 123, BufferSize(input_dims));
    device_.memset(filter_data, 123, BufferSize(filter_dims));

    Input input(input_data, input_dims);
    Filter filter(filter_data, filter_dims);
    Output output(output_data, output_dims);

    ::tensorflow::testing::StartTiming();
    for (int i = 0; i < iters_; ++i) {
      output.device(device_) = Eigen::SpatialConvolutionBackwardInput(
          filter, input, input_rows, input_cols);
      tensorflow::testing::DoNotOptimize(output);
    }
    ::tensorflow::testing::StopTiming();

    device_.deallocate(input_data);
    device_.deallocate(filter_data);
    device_.deallocate(output_data);
  }

 private:
  int iters_;
  Device& device_;
};

#endif  // TENSORFLOW_CORE_KERNELS_EIGEN_BENCHMARK_H_
