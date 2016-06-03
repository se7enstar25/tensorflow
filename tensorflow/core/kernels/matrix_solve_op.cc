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

// See docs in ../ops/linalg_ops.cc.
// TODO(rmlarsen): Add optional hint params so the caller can promise that the
// matrices are invertible, symmetric (maybe detect automatically?), and
// positive definite, which will allow us to call progressively faster solvers
// internally.
#include <cmath>

#include "third_party/eigen3/Eigen/LU"
#include "tensorflow/core/framework/kernel_def_builder.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/kernels/binary_linalg_ops_common.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {

template <class Scalar, bool SupportsBatchOperationT>
class MatrixSolveOp
    : public BinaryLinearAlgebraOp<Scalar, SupportsBatchOperationT> {
 public:
  explicit MatrixSolveOp(OpKernelConstruction* context)
      : BinaryLinearAlgebraOp<Scalar, SupportsBatchOperationT>(context) {
    OP_REQUIRES_OK(context, context->GetAttr("adjoint", &adjoint_));
  }
  ~MatrixSolveOp() override {}

  TensorShape GetOutputMatrixShape(
      const TensorShape& input_matrix_shape,
      const TensorShape& rhs_matrix_shape) override {
    CHECK_EQ(input_matrix_shape.dims(), rhs_matrix_shape.dims());
    TensorShape output_matrix_shape = input_matrix_shape;
    output_matrix_shape.set_dim(
        output_matrix_shape.dims() - 1,
        rhs_matrix_shape.dim_size(output_matrix_shape.dims() - 1));
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
      return rows * rows * (rows + rhss);
    }
  }

  using typename BinaryLinearAlgebraOp<Scalar, SupportsBatchOperationT>::Matrix;
  using typename BinaryLinearAlgebraOp<Scalar,
                                       SupportsBatchOperationT>::MatrixMap;
  using typename BinaryLinearAlgebraOp<Scalar,
                                       SupportsBatchOperationT>::ConstMatrixMap;

  void ComputeMatrix(OpKernelContext* context, const ConstMatrixMap& matrix,
                     const ConstMatrixMap& rhs, MatrixMap* output) override {
    OP_REQUIRES(context, matrix.rows() == matrix.cols(),
                errors::InvalidArgument("Input matrix must be square."));
    OP_REQUIRES(
        context, matrix.cols() == rhs.rows(),
        errors::InvalidArgument("Input matrix and rhs are incompatible."));
    if (matrix.rows() == 0 || rhs.cols() == 0) {
      // To be consistent with the MatrixInverse op, we define the solution for
      // an empty set of equation as the empty matrix.
      return;
    }
    Eigen::PartialPivLU<Matrix> lu_decomposition(matrix.rows());
    if (adjoint_) {
      // TODO(rmlarsen): For Eigen 3.2, this creates a temporary copy.
      // Make sure to backport: https://bitbucket.org/eigen/eigen/commits/ \
      // bd2219a74c96dfe3f6bc2c23588749e36d2d8173
      lu_decomposition.compute(matrix.adjoint());
    } else {
      lu_decomposition.compute(matrix);
    }

    // PartialPivLU cannot give strong guarantees on invertibility,
    // but we can at least guard against exact zero pivots. This can occur as
    // a result of basic user mistakes such providing integer valued
    // matrices that are exactly singular, or due to underflow if this
    // code is run with denormals being flushed to zero.
    const Scalar min_abs_pivot =
        lu_decomposition.matrixLU().diagonal().cwiseAbs().minCoeff();
    OP_REQUIRES(context, min_abs_pivot > Scalar(0),
                errors::InvalidArgument("Input matrix is not invertible."));

    // TODO(rmlarsen): Add check based on condition number estimation.
    // The necessary changes to Eigen are in
    // https://bitbucket.org/eigen/eigen/pull-requests/174/ \
    // add-matrix-condition-number-estimation/diff
    *output = lu_decomposition.solve(rhs);
  }

 private:
  bool adjoint_;

  TF_DISALLOW_COPY_AND_ASSIGN(MatrixSolveOp);
};

REGISTER_BINARY_LINALG_OP("MatrixSolve", (MatrixSolveOp<float, false>), float);
REGISTER_BINARY_LINALG_OP("MatrixSolve", (MatrixSolveOp<double, false>),
                          double);
REGISTER_BINARY_LINALG_OP("BatchMatrixSolve", (MatrixSolveOp<float, true>),
                          float);
REGISTER_BINARY_LINALG_OP("BatchMatrixSolve", (MatrixSolveOp<double, true>),
                          double);

}  // namespace tensorflow
