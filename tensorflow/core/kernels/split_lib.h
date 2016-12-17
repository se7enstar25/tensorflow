/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_KERNELS_SPLIT_LIB_H_
#define TENSORFLOW_KERNELS_SPLIT_LIB_H_
// Functor definition for SplitOp, must be compilable by nvcc.

#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"
#include "tensorflow/core/framework/tensor_types.h"

namespace tensorflow {
namespace functor {

template <typename Device, typename T>
struct SplitCustom {
  void operator()(const Device& d, typename TTypes<T, 2>::Tensor output,
                  typename TTypes<T, 2>::ConstTensor input,
                  const Eigen::DSizes<Eigen::DenseIndex, 2>& slice_indices,
                  const Eigen::DSizes<Eigen::DenseIndex, 2>& slice_sizes);
};

template <typename Device, typename T>
struct Split {
  void operator()(const Device& d, typename TTypes<T, 3>::Tensor output,
                  typename TTypes<T, 3>::ConstTensor input,
                  const Eigen::DSizes<Eigen::DenseIndex, 3>& slice_indices,
                  const Eigen::DSizes<Eigen::DenseIndex, 3>& slice_sizes);
};

template <typename T>
struct Split<Eigen::ThreadPoolDevice, T> {
  void operator()(const Eigen::ThreadPoolDevice& d,
                  typename TTypes<T, 3>::Tensor output,
                  typename TTypes<T, 3>::ConstTensor input,
                  const Eigen::DSizes<Eigen::DenseIndex, 3>& slice_indices,
                  const Eigen::DSizes<Eigen::DenseIndex, 3>& slice_sizes);
};

#ifdef TENSORFLOW_USE_SYCL
template <typename T>
struct Split<Eigen::SyclDevice, T> {
  void operator()(const Eigen::SyclDevice& d,
                  typename TTypes<T, 3>::Tensor output,
                  typename TTypes<T, 3>::ConstTensor input,
                  const Eigen::DSizes<Eigen::DenseIndex, 3>& slice_indices,
                  const Eigen::DSizes<Eigen::DenseIndex, 3>& slice_sizes);
};
#endif // TENSORFLOW_USE_SYCL

}  // namespace functor
}  // namespace tensorflow

#endif  // TENSORFLOW_KERNELS_SPLIT_LIB_H_
