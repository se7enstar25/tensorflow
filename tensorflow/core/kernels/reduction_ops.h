/* Copyright 2015 Google Inc. All Rights Reserved.

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

#ifndef TENSORFLOW_KERNELS_REDUCTION_OPS_H_
#define TENSORFLOW_KERNELS_REDUCTION_OPS_H_

// Functor definitions for Reduction ops, must be compilable by nvcc.

#include <iostream>
#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"
#include "tensorflow/core/framework/tensor_types.h"

namespace tensorflow {
namespace functor {

// When eigen3 has better implementation of AllReducer and AnyReducer,
// replaces reducers here.

// Reduction using logical_and.
struct AllReducer {
  // TODO(zhifengc): Implement PacketAccess when performance matters.
  static const bool PacketAccess = false;
  static const bool IsStateful = false;

  EIGEN_DEVICE_FUNC void reduce(const bool t, bool* accum) const {
    *accum &= t;
  }

  EIGEN_DEVICE_FUNC bool initialize() const { return true; }

  EIGEN_DEVICE_FUNC bool finalize(const bool accum) const { return accum; }
};

// Reduction using logical_or.
struct AnyReducer {
  // TODO(zhifengc): Implement PacketAccess when performance matters.
  static const bool PacketAccess = false;
  static const bool IsStateful = false;

  EIGEN_DEVICE_FUNC void reduce(const bool t, bool* accum) const {
    *accum |= t;
  }

  EIGEN_DEVICE_FUNC bool initialize() const { return false; }

  EIGEN_DEVICE_FUNC bool finalize(const bool accum) const { return accum; }
};

template <typename Device, typename OUT_T, typename IN_T,
          typename ReductionAxes, typename Reducer>
void ReduceEigenImpl(const Device& d, OUT_T out, IN_T in,
                     const ReductionAxes& reduction_axes,
                     const Reducer& reducer) {
  out.device(d) = in.reduce(reduction_axes, reducer);
}

template <typename Device>
struct ReduceFunctor {
  template <typename OUT_T, typename IN_T, typename ReductionAxes,
            typename Reducer>
  static void Reduce(const Device& d, OUT_T out, IN_T in,
                     const ReductionAxes& reduction_axes,
                     const Reducer& reducer);
};

}  // namespace functor
}  // namespace tensorflow

#endif  // TENSORFLOW_KERNELS_REDUCTION_OPS_H_
