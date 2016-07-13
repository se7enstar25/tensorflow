/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_KERNELS_SCAN_OPS_H_
#define TENSORFLOW_KERNELS_SCAN_OPS_H_

#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"
#include "tensorflow/core/framework/tensor_types.h"

namespace tensorflow {
namespace functor {

typedef Eigen::Index Index;

template <typename Device, typename Reducer, typename T, int Dims>
struct Scan {
  void operator()(const Device& d, typename TTypes<T, Dims>::ConstTensor in,
                  typename TTypes<T, Dims>::Tensor out, const Reducer& reducer,
                  const Index& axis, bool reverse) {
    if (reverse) {
      // Perform the reverse ops directly with Eigen, which avoids copying the
      // tensor twice compared to executing this as individual ops.
      Eigen::array<bool, Dims> dims;
      for (int i = 0; i < dims.size(); i++) {
        dims[i] = (i == axis);
      }
      To32Bit(out).device(d) = To32Bit(in).reverse(dims)
                                          .scan(axis, reducer)
                                          .reverse(dims);
    } else {
      To32Bit(out).device(d) = To32Bit(in).scan(axis, reducer);
    }
  }
};

}  // namespace functor
}  // namespace tensorflow

#endif  // TENSORFLOW_KERNELS_SCAN_OPS_H_
