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

// See docs in ../ops/linalg_ops.cc.
#include <cmath>

#include "third_party/eigen3/Eigen/Cholesky"
#include "third_party/eigen3/Eigen/Core"
#include "third_party/eigen3/Eigen/QR"
#include "tensorflow/core/framework/kernel_def_builder.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/kernels/binary_linalg_ops_common.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {

template <class Scalar, bool SupportsBatchOperationT>
class MatrixSolveLsOp
    : public BinaryLinearAlgebraOp<Scalar, SupportsBatchOperationT> {
 public:
  explicit MatrixSolveLsOp(OpKernelConstruction* context)
      : BinaryLinearAlgebraOp<Scalar, SupportsBatchOperationT>(context) {
    OP_REQUIRES_OK(context, context->GetAttr("fast", &fast_));
  }

  ~MatrixSolveLsOp() override {}

  TensorShape GetOutputMatrixShape(
      const TensorShape& input_matrix_shape,
      const TensorShape& rhs_matrix_shape) override {
    CHECK_EQ(input_matrix_shape.dims(), rhs_matrix_shape.dims());
    TensorShape output_matrix_shape = rhs_matrix_shape;
    output_matrix_shape.set_dim(
        output_matrix_shape.dims() - 2,
        input_matrix_shape.dim_size(output_matrix_shape.dims() - 1));
    return output_matrix_shape;
  }

  int64 GetCostPerUnit(const TensorShape& input_matrix_shape,
                       const TensorShape& rhs_matrix_shape) override {
    const int64 rows = input_matrix_shape.dim_size(0);
    const int64 rhss = rhs_matrix_shape.dim_size(1);
    if (rows > (1LL << 20)) {
      // A big number to cap the cost in case overflow.
      return kint32max;
    } else {
      return 2 * rows * rows * (rows + rhss);
    }
  }

  using typename BinaryLinearAlgebraOp<Scalar, SupportsBatchOperationT>::Matrix;
  using typename BinaryLinearAlgebraOp<Scalar,
                                       SupportsBatchOperationT>::MatrixMap;
  using typename BinaryLinearAlgebraOp<Scalar,
                                       SupportsBatchOperationT>::ConstMatrixMap;

  void ComputeMatrix(OpKernelContext* context, const ConstMatrixMap& matrix,
                     const ConstMatrixMap& rhs, MatrixMap* output) override {
    const int64 rows = matrix.rows();
    const int64 cols = matrix.cols();
    OP_REQUIRES(
        context, rows == rhs.rows(),
        errors::InvalidArgument("Input matrix and rhs are incompatible."));
    const auto& l2_regularizer_in = context->input(2);
    OP_REQUIRES(
        context, TensorShapeUtils::IsScalar(l2_regularizer_in.shape()),
        errors::InvalidArgument("l2_regularizer must be scalar, got shape ",
                                l2_regularizer_in.shape().DebugString()));
    const double l2_regularizer = l2_regularizer_in.scalar<double>()();

    OP_REQUIRES(context, l2_regularizer >= 0,
                errors::InvalidArgument("l2_regularizer must be >= 0."));
    if (rows == 0 || cols == 0) {
      // The result is the empty matrix.
      return;
    }
    if (fast_) {
      // The fast branch assumes that matrix is not rank deficient and
      // not too ill-conditioned. Specifically, the reciprobal condition number
      // should be greater than the square root of the machine precision, i.e.
      //   1 / cond(matrix) > sqrt(std::numeric_limits<Scalar>::epsilon()).
      // This branch solves over- or underdetermined least-squares problems
      // via the normal equations and Cholesky decomposition.
      if (matrix.rows() >= matrix.cols()) {
        // Overdetermined case (rows >= cols): Solves the ordinary (possibly
        // regularized) least-squares problem
        //   min || A * X - RHS ||_F^2 + l2_regularizer ||X||_F^2
        // by solving the normal equations
        //    (A^T * A + l2_regularizer * I) X = A^T RHS
        // using Cholesky decomposition.
        Matrix gramian(cols, cols);
        gramian.template triangularView<Eigen::Lower>() =
            matrix.transpose() * matrix;
        if (l2_regularizer > 0) {
          gramian +=
              (Scalar(l2_regularizer) * Matrix::Ones(cols, 1)).asDiagonal();
        }
        const Eigen::LLT<Matrix, Eigen::Lower> llt(gramian);
        OP_REQUIRES(
            context, llt.info() == Eigen::Success,
            errors::InvalidArgument("Input matrix was rank deficient or "
                                    "ill-conditioned. Try setting fast=False "
                                    "or provide a larger l2_regularizer > 0."));
        *output = llt.solve(matrix.transpose() * rhs);
      } else {
        // Underdetermined case (rows < cols): Solves the minimum-norm problem
        //   min ||X||_F^2 s.t. A*X = RHS
        // by solving the normal equations of the second kind
        //   (A * A^T + l2_regularizer * I) Z = RHS,  X = A^T * Z
        // using Cholesky decomposition.
        Matrix gramian(rows, rows);
        gramian.template triangularView<Eigen::Lower>() =
            matrix * matrix.transpose();
        if (l2_regularizer > 0) {
          gramian +=
              (Scalar(l2_regularizer) * Matrix::Ones(rows, 1)).asDiagonal();
        }
        const Eigen::LLT<Matrix, Eigen::Lower> llt(gramian);
        OP_REQUIRES(
            context, llt.info() == Eigen::Success,
            errors::InvalidArgument("Input matrix was rank deficient or "
                                    "ill-conditioned. Try setting fast=False "
                                    "or provide an l2_regularizer > 0."));
        *output = matrix.transpose() * llt.solve(rhs);
      }
    } else {
      // Use a rank revealing factorization (QR with column pivoting).
      //
      // NOTICE: Currently, Eigen's implementation of column pivoted Householder
      // QR has a few deficiencies:
      //  1. It does not implement the post-processing step to compute a
      //     complete orthogonal factorization. This means that it does not
      //     return a minimum-norm solution for underdetermined and
      //     rank-deficient matrices. We could use the Eigen SVD instead, but
      //     the currently available JacobiSVD is so slow that is it is
      //     essentially useless (~100x slower than QR).
      //  2. The implementation is not blocked, so for matrics that do not fit
      //     in cache, it is significantly slower than the equivalent blocked
      //     LAPACK routine xGEQP3 (e.g. Eigen is ~3x slower for 4k x 4k
      //     matrices). See http://www.netlib.org/lapack/lawnspdf/lawn114.pdf
      //  3. The implementation uses the numerically unstable norm downdating
      //     formula from the original 1965 Businger & Golub paper. This can
      //     lead to incorrect rank determination for graded matrices. I
      //     (rmlarsen@) have a patch to bring this up to date by implementing
      //     the robust formula from
      //     http://www.netlib.org/lapack/lawnspdf/lawn176.pdf
      //
      // TODO(rmlarsen): a) Contribute 1. and 2. to Eigen.
      //                 b) Evaluate new divide-and-conquer SVD in Eigen when
      //                    it becomes available & robust.
      *output = matrix.colPivHouseholderQr().solve(rhs);
    }
  }

 private:
  bool fast_;
};

REGISTER_BINARY_LINALG_OP("MatrixSolveLs", (MatrixSolveLsOp<float, false>),
                          float);
REGISTER_BINARY_LINALG_OP("MatrixSolveLs", (MatrixSolveLsOp<double, false>),
                          double);
REGISTER_BINARY_LINALG_OP("BatchMatrixSolveLs", (MatrixSolveLsOp<float, true>),
                          float);
REGISTER_BINARY_LINALG_OP("BatchMatrixSolveLs", (MatrixSolveLsOp<double, true>),
                          double);

}  // namespace tensorflow
