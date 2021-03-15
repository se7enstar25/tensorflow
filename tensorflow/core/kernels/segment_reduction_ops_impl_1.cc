/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

// See docs in ../ops/math_ops.cc.
#include "tensorflow/core/kernels/segment_reduction_ops_impl.h"

namespace tensorflow {
namespace internal {
// Static routines not in the templated class to reduce code size
void SegmentReductionValidationHelper(OpKernelContext* context,
                                      const Tensor& input,
                                      const Tensor& segment_ids) {
  OP_REQUIRES(context, TensorShapeUtils::IsVectorOrHigher(input.shape()),
              errors::InvalidArgument("input must be at least rank 1"));
  OP_REQUIRES(context, TensorShapeUtils::IsVector(segment_ids.shape()),
              errors::InvalidArgument("segment_ids should be a vector."));
  const int64 num_indices = segment_ids.NumElements();
  OP_REQUIRES(context, num_indices == input.dim_size(0),
              errors::InvalidArgument(
                  "segment_ids should be the same size as dimension 0 of"
                  " input."));
}

bool SegmentReductionDoValidation(OpKernelContext* c, const Tensor& input,
                                  const Tensor& segment_ids) {
  SegmentReductionValidationHelper(c, input, segment_ids);
  return c->status().ok();
}

// check routines not in the templated class to reduce code size
void UnsortedSegmentReductionValidation(OpKernel* op_kernel,
                                        OpKernelContext* context,
                                        const Tensor& data,
                                        const Tensor& segment_ids,
                                        const Tensor& num_segments) {
  OP_REQUIRES(
      context, TensorShapeUtils::IsScalar(num_segments.shape()),
      errors::InvalidArgument("num_segments should be a scalar, not shape ",
                              num_segments.shape().DebugString()));
  OP_REQUIRES(
      context, TensorShapeUtils::StartsWith(data.shape(), segment_ids.shape()),
      errors::InvalidArgument("data.shape = ", data.shape().DebugString(),
                              " does not start with segment_ids.shape = ",
                              segment_ids.shape().DebugString()));
}

bool UnsortedSegmentReductionDoValidation(OpKernel* op_kernel,
                                          OpKernelContext* context,
                                          const Tensor& data,
                                          const Tensor& segment_ids,
                                          const Tensor& num_segments) {
  UnsortedSegmentReductionValidation(op_kernel, context, data, segment_ids,
                                     num_segments);
  return context->status().ok();
}

void SparseSegmentReductionValidationHelper(OpKernelContext* context,
                                            const Tensor& input,
                                            const Tensor& indices,
                                            const Tensor& segment_ids,
                                            bool has_num_segments,
                                            AsyncOpKernel::DoneCallback done) {
  if (!done) {
    done = [] {};
  }
  if (has_num_segments) {
    const Tensor& num_segments = context->input(3);
    OP_REQUIRES_ASYNC(
        context, TensorShapeUtils::IsScalar(num_segments.shape()),
        errors::InvalidArgument("num_segments should be a scalar, not shape ",
                                num_segments.shape().DebugString()),
        done);
    // Note that there is a Tnumsegments parameter on the op, but it is not
    // plumbed through to here and so always takes its default value of int32.
    int32 output_rows =
        internal::SubtleMustCopy(num_segments.scalar<int32>()());
    OP_REQUIRES_ASYNC(context, output_rows >= 0,
                      errors::InvalidArgument("segment ids must be >= 0"),
                      done);
  }
  OP_REQUIRES_ASYNC(context, TensorShapeUtils::IsVector(indices.shape()),
                    errors::InvalidArgument("indices should be a vector."),
                    done);
  OP_REQUIRES_ASYNC(context, TensorShapeUtils::IsVector(segment_ids.shape()),
                    errors::InvalidArgument("segment_ids should be a vector."),
                    done);
  const int64 num_indices = indices.NumElements();
  OP_REQUIRES_ASYNC(
      context, num_indices == segment_ids.NumElements(),
      errors::InvalidArgument("segment_ids and indices should have same size."),
      done);
}

bool SparseSegmentReductionDoValidation(OpKernelContext* c, const Tensor& input,
                                        const Tensor& indices,
                                        const Tensor& segment_ids,
                                        bool has_num_segments,
                                        AsyncOpKernel::DoneCallback done) {
  SparseSegmentReductionValidationHelper(c, input, indices, segment_ids,
                                         has_num_segments, done);
  return c->status().ok();
}

}  // namespace internal

#define REGISTER_CPU_KERNEL_SEGMENT(name, functor, type, index_type, \
                                    default_value)                   \
  REGISTER_KERNEL_BUILDER(                                           \
      Name(name)                                                     \
          .Device(DEVICE_CPU)                                        \
          .TypeConstraint<type>("T")                                 \
          .TypeConstraint<index_type>("Tindices"),                   \
      SegmentReductionOp<CPUDevice, type, index_type, functor, default_value>)

#define REGISTER_REAL_CPU_KERNELS(type, index_type)                            \
  REGISTER_CPU_KERNEL_SEGMENT("SegmentSum", Eigen::internal::SumReducer<type>, \
                              type, index_type, 0);                            \
  REGISTER_CPU_KERNEL_SEGMENT(                                                 \
      "SegmentMean", Eigen::internal::MeanReducer<type>, type, index_type, 0); \
  REGISTER_CPU_KERNEL_SEGMENT(                                                 \
      "SegmentProd", Eigen::internal::ProdReducer<type>, type, index_type, 1); \
  REGISTER_CPU_KERNEL_SEGMENT("SegmentMin", Eigen::internal::MinReducer<type>, \
                              type, index_type, 0);                            \
  REGISTER_CPU_KERNEL_SEGMENT("SegmentMax", Eigen::internal::MaxReducer<type>, \
                              type, index_type, 0)

#define REGISTER_COMPLEX_CPU_KERNELS(type, index_type)                         \
  REGISTER_CPU_KERNEL_SEGMENT("SegmentSum", Eigen::internal::SumReducer<type>, \
                              type, index_type, 0);                            \
  REGISTER_CPU_KERNEL_SEGMENT(                                                 \
      "SegmentMean", Eigen::internal::MeanReducer<type>, type, index_type, 0); \
  REGISTER_CPU_KERNEL_SEGMENT(                                                 \
      "SegmentProd", Eigen::internal::ProdReducer<type>, type, index_type, 1);

#define REGISTER_REAL_CPU_KERNELS_ALL(type) \
  REGISTER_REAL_CPU_KERNELS(type, int32)

#define REGISTER_COMPLEX_CPU_KERNELS_ALL(type) \
  REGISTER_COMPLEX_CPU_KERNELS(type, int32)

TF_CALL_REAL_NUMBER_TYPES(REGISTER_REAL_CPU_KERNELS_ALL);
REGISTER_COMPLEX_CPU_KERNELS_ALL(complex64);
REGISTER_COMPLEX_CPU_KERNELS_ALL(complex128);
#undef REGISTER_CPU_KERNEL_SEGMENT
#undef REGISTER_REAL_CPU_KERNELS
#undef REGISTER_COMPLEX_CPU_KERNELS
#undef REGISTER_REAL_CPU_KERNELS_ALL
#undef REGISTER_COMPLEX_CPU_KERNELS_ALL

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM
#define REGISTER_GPU_KERNEL_SORTEDSEGMENT(                                   \
    name, type, index_type, initial_value_functor, reduction_kernel_functor, \
    atomic_reduction_kernel_functor)                                         \
  REGISTER_KERNEL_BUILDER(                                                   \
      Name(name)                                                             \
          .Device(DEVICE_GPU)                                                \
          .TypeConstraint<type>("T")                                         \
          .TypeConstraint<index_type>("Tindices"),                           \
      SegmentReductionGPUOp<                                                 \
          type, index_type,                                                  \
          functor::SegmentReductionFunctor<                                  \
              type, index_type, initial_value_functor,                       \
              reduction_kernel_functor, atomic_reduction_kernel_functor> >)

#define REGISTER_GPU_SORTED_KERNELS(type, index_type)                     \
  REGISTER_GPU_KERNEL_SORTEDSEGMENT(                                      \
      "SegmentSum", type, index_type, functor::Zero<type>,                \
      functor::NonAtomicSumOpGpu<type>, functor::AtomicSumOpGpu<type>);   \
  REGISTER_GPU_KERNEL_SORTEDSEGMENT(                                      \
      "SegmentProd", type, index_type, functor::One<type>,                \
      functor::NonAtomicProdOpGpu<type>, functor::AtomicProdOpGpu<type>); \
  REGISTER_GPU_KERNEL_SORTEDSEGMENT(                                      \
      "SegmentMin", type, index_type, functor::Highest<type>,             \
      functor::NonAtomicMinOpGpu<type>, functor::AtomicMinOpGpu<type>);   \
  REGISTER_GPU_KERNEL_SORTEDSEGMENT(                                      \
      "SegmentMax", type, index_type, functor::Lowest<type>,              \
      functor::NonAtomicMaxOpGpu<type>, functor::AtomicMaxOpGpu<type>);

#define REGISTER_GPU_SORTED_KERNELS_ALL(type) \
  REGISTER_GPU_SORTED_KERNELS(type, int32)

TF_CALL_GPU_NUMBER_TYPES(REGISTER_GPU_SORTED_KERNELS_ALL);
#undef REGISTER_GPU_KERNEL_SORTEDSEGMENT
#undef REGISTER_GPU_SORTED_KERNELS
#undef REGISTER_GPU_SORTED_KERNELS_ALL
#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM

}  // namespace tensorflow
