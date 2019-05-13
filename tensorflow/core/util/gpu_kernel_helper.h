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

#ifndef TENSORFLOW_CORE_UTIL_GPU_KERNEL_HELPER_H_
#define TENSORFLOW_CORE_UTIL_GPU_KERNEL_HELPER_H_

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM

#if GOOGLE_CUDA
#include "third_party/gpus/cuda/include/cuda_fp16.h"
#endif
#include "tensorflow/core/util/gpu_device_functions.h"
#include "tensorflow/core/util/gpu_launch_config.h"

#if GOOGLE_CUDA
#define TF_RED_WARPSIZE 32
#elif TENSORFLOW_USE_ROCM
#define TF_RED_WARPSIZE 64
#endif

// Deprecated, use 'for(int i : CudaGridRangeX(n))' instead.
#define CUDA_1D_KERNEL_LOOP(i, n) \
  for (int i : ::tensorflow::CudaGridRangeX<int>(n))
// Deprecated, use 'for(int i : CudaGridRange?(n))' instead.
#define CUDA_AXIS_KERNEL_LOOP(i, n, axis) \
  for (int i : ::tensorflow::CudaGridRange##axis<int>(n))

#if GOOGLE_CUDA
#define gpuSuccess cudaSuccess
using gpuStream_t = cudaStream_t;
using gpuError_t = cudaError_t;
#elif TENSORFLOW_USE_ROCM
#define gpuSuccess hipSuccess
using gpuStream_t = hipStream_t;
using gpuError_t = hipError_t;
#endif

namespace tensorflow {
#if GOOGLE_CUDA
// cudaGetErrorString is available to both host and device
__host__ __device__ inline const char* GpuGetErrorString(cudaError_t error) {
  return cudaGetErrorString(error);
#elif TENSORFLOW_USE_ROCM
// hipGetErrorString is available on host side only
inline const char* GpuGetErrorString(hipError_t error) {
  return hipGetErrorString(error);
#endif
}

inline const gpuStream_t& GetGpuStream(OpKernelContext* context) {
  // Returns a raw reference to the current cuda stream. Required by a
  // number of kernel calls (for which StreamInterface* does not work),
  // i.e. CUB and certain cublas primitives.
  const gpuStream_t* ptr = CHECK_NOTNULL(
      reinterpret_cast<const gpuStream_t*>(context->op_device_context()
                                               ->stream()
                                               ->implementation()
                                               ->GpuStreamMemberHack()));
  return *ptr;
}

__host__ __device__ inline tensorflow::bfloat16 CudaLdg(
    const tensorflow::bfloat16* address) {
  tensorflow::bfloat16 return_value;
  return_value.value = CudaLdg(reinterpret_cast<const uint16_t*>(address));
  return return_value;
}

template <typename T>
__host__ __device__ inline T ldg(const T* ptr) {
  return CudaLdg(ptr);
}

template <typename T>
__host__ __device__ inline const T& tf_min(const T& x, const T& y) {
  return x < y ? x : y;
}

template <typename T>
__host__ __device__ inline const T& tf_max(const T& x, const T& y) {
  return x < y ? y : x;
}

// Overloads of the above functions for float and double.
__host__ __device__ inline float tf_min(float x, float y) {
  return fminf(x, y);
}
__host__ __device__ inline double tf_min(double x, double y) {
  return fmin(x, y);
}
__host__ __device__ inline float tf_max(float x, float y) {
  return fmaxf(x, y);
}
__host__ __device__ inline double tf_max(double x, double y) {
  return fmax(x, y);
}

__device__ inline Eigen::half CudaShuffleSync(unsigned mask, Eigen::half value,
                                              int src_lane,
                                              int width = warpSize) {
  return Eigen::half(
      CudaShuffleSync(mask, static_cast<uint16>(value), src_lane, width));
}

__device__ EIGEN_ALWAYS_INLINE Eigen::half CudaShuffleUpSync(
    unsigned mask, Eigen::half value, int delta, int width = warpSize) {
  return Eigen::half(
      CudaShuffleUpSync(mask, static_cast<uint16>(value), delta, width));
}

__device__ EIGEN_ALWAYS_INLINE Eigen::half CudaShuffleDownSync(
    unsigned mask, Eigen::half value, int delta, int width = warpSize) {
  return Eigen::half(
      CudaShuffleDownSync(mask, static_cast<uint16>(value), delta, width));
}

__device__ EIGEN_ALWAYS_INLINE Eigen::half CudaShuffleXorSync(
    unsigned mask, Eigen::half value, int lane_mask, int width = warpSize) {
  return Eigen::half(
      CudaShuffleXorSync(mask, static_cast<uint16>(value), lane_mask, width));
}

namespace cuda_helper {
template <typename T, typename OutType = int32>
__device__ OutType upper_bound(const T* first, OutType count, T val) {
  const T* orig = first;
  const T* it = nullptr;
  OutType step = 0;
  while (count > 0) {
    it = first;
    step = count / 2;
    it += step;
    if (!(val < *it)) {
      first = ++it;
      count -= step + 1;
    } else {
      count = step;
    }
  }

  return first - orig;
}

template <typename T, typename OutType = int32>
__device__ OutType lower_bound(const T* first, OutType count, T val) {
  const T* orig = first;
  const T* it = nullptr;
  OutType step = 0;
  while (count > 0) {
    it = first;
    step = count / 2;
    it += step;
    if (*it < val) {
      first = ++it;
      count -= step + 1;
    } else {
      count = step;
    }
  }

  return first - orig;
}

}  // namespace cuda_helper
}  // namespace tensorflow

#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM
#endif  // TENSORFLOW_CORE_UTIL_GPU_KERNEL_HELPER_H_
