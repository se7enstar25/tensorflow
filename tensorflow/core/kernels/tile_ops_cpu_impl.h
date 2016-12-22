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
#ifndef THIRD_PARTY_TENSORFLOW_CORE_KERNELS_TILE_OPS_CPU_IMPL_H_
#define THIRD_PARTY_TENSORFLOW_CORE_KERNELS_TILE_OPS_CPU_IMPL_H_

#define EIGEN_USE_THREADS

#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/kernels/tile_ops_impl.h"

namespace tensorflow {
namespace functor {

typedef Eigen::ThreadPoolDevice CPUDevice;

// Register functors used for TileOp.
#define DEFINE_DIM(T, NDIM) template struct Tile<CPUDevice, T, NDIM>;
#define DEFINE_TYPE(T) DEFINE_DIM(T, CPU_PROVIDED_IXDIM)

TF_CALL_bool(DEFINE_TYPE);
TF_CALL_float(DEFINE_TYPE);
TF_CALL_double(DEFINE_TYPE);
TF_CALL_uint8(DEFINE_TYPE);
TF_CALL_int32(DEFINE_TYPE);
TF_CALL_int16(DEFINE_TYPE);
TF_CALL_int64(DEFINE_TYPE);
TF_CALL_half(DEFINE_TYPE);
TF_CALL_complex64(DEFINE_TYPE);
TF_CALL_complex128(DEFINE_TYPE);
TF_CALL_string(DEFINE_TYPE);

#undef DEFINE_DIM
#undef DEFINE_TYPE

// Register functors used for TileGradientOp.
#define DEFINE_DIM(T, NDIM)                     \
  template struct TileGrad<CPUDevice, T, NDIM>; \
  template struct ReduceAndReshape<CPUDevice, T, NDIM, 1>;
#define DEFINE_TYPE(T) DEFINE_DIM(T, CPU_PROVIDED_IXDIM)

TF_CALL_float(DEFINE_TYPE);
TF_CALL_double(DEFINE_TYPE);
TF_CALL_int16(DEFINE_TYPE);
TF_CALL_int32(DEFINE_TYPE);
TF_CALL_int64(DEFINE_TYPE);
TF_CALL_half(DEFINE_TYPE);
TF_CALL_complex64(DEFINE_TYPE);
TF_CALL_complex128(DEFINE_TYPE);

#undef DEFINE_DIM
#undef DEFINE_TYPE

#ifdef TENSORFLOW_USE_SYCL
typedef Eigen::SyclDevice SYCLDevice;

// Register functors used for TileOp.
#define DEFINE_DIM(T, NDIM) template struct Tile<SYCLDevice, T, NDIM>;
#define DEFINE_TYPE(T) DEFINE_DIM(T, CPU_PROVIDED_IXDIM)

TF_CALL_float(DEFINE_TYPE);

#undef DEFINE_DIM
#undef DEFINE_TYPE

// Register functors used for TileGradientOp.
#define DEFINE_DIM(T, NDIM)                      \
  template struct TileGrad<SYCLDevice, T, NDIM>; \
  template struct ReduceAndReshape<SYCLDevice, T, NDIM, 1>;
#define DEFINE_TYPE(T) DEFINE_DIM(T, CPU_PROVIDED_IXDIM)

TF_CALL_float(DEFINE_TYPE);

#undef DEFINE_DIM
#undef DEFINE_TYPE
#endif // TENSORFLOW_USE_SYCL

}  // end namespace functor
}  // end namespace tensorflow

#endif  // THIRD_PARTY_TENSORFLOW_CORE_KERNELS_TILE_OPS_CPU_IMPL_H_
