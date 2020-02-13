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

#include "tensorflow/core/kernels/eigen_backward_cuboid_convolutions.h"

#include "tensorflow/core/platform/test.h"

namespace Eigen {

namespace {
void EigenApprox(float a, float b) {
  ASSERT_TRUE(std::abs(a - b) <= std::min(std::abs(a), std::abs(b)) * 1e-3);
}
static int ceil_div(int a, int b) { return (a + b - 1) / b; }
}  // namespace

TEST(EigenBackwardSpatialConvolutionsTest,
     test_simple_cuboid_convolution_backward_input_valid) {
  const int input_depth = 2;
  const int input_planes = 5;
  const int input_rows = 3;
  const int input_cols = 4;
  const int patch_rows = 2;
  const int patch_cols = 2;
  const int patch_planes = 2;
  const int output_rows = input_rows - patch_rows + 1;
  const int output_cols = input_cols - patch_cols + 1;
  const int output_planes = input_planes - patch_planes + 1;
  const int output_depth = 5;

  Tensor<float, 4> input_backward(input_depth, input_planes, input_rows,
                                  input_cols);
  Tensor<float, 5> kernel(output_depth, input_depth, patch_planes, patch_rows,
                          patch_cols);
  Tensor<float, 4> output_backward(output_depth, output_planes, output_rows,
                                   output_cols);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  kernel = kernel.constant(2.0f) + kernel.random();
  input_backward.setRandom();

  input_backward = CuboidConvolutionBackwardInput(
      kernel, output_backward, input_planes, input_rows, input_cols);

  EXPECT_EQ(input_backward.dimension(3), input_cols);
  EXPECT_EQ(input_backward.dimension(2), input_rows);
  EXPECT_EQ(input_backward.dimension(1), input_planes);
  EXPECT_EQ(input_backward.dimension(0), input_depth);

  for (int id = 0; id < input_depth; ++id) {
    for (int i = 0; i < input_planes; ++i) {
      for (int j = 0; j < input_rows; ++j) {
        for (int k = 0; k < input_cols; ++k) {
          float expected = 0.0f;
          for (int c = 0; c < patch_cols; ++c) {
            for (int r = 0; r < patch_rows; ++r) {
              for (int p = 0; p < patch_planes; ++p) {
                for (int od = 0; od < output_depth; ++od) {
                  int output_j = j - r;
                  int output_k = k - c;
                  int output_i = i - p;
                  if (output_i >= 0 && output_i < output_planes &&
                      output_j >= 0 && output_j < output_rows &&
                      output_k >= 0 && output_k < output_cols) {
                    expected +=
                        output_backward(od, output_i, output_j, output_k) *
                        kernel(od, id, p, r, c);
                  }
                }
              }
            }
          }
          EigenApprox(input_backward(id, i, j, k), expected);
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_simple_cuboid_convolution_backward_input_valid_row_major) {
  const int input_depth = 2;
  const int input_planes = 5;
  const int input_rows = 3;
  const int input_cols = 4;
  const int patch_rows = 2;
  const int patch_cols = 2;
  const int patch_planes = 2;
  const int output_rows = input_rows - patch_rows + 1;
  const int output_cols = input_cols - patch_cols + 1;
  const int output_planes = input_planes - patch_planes + 1;
  const int output_depth = 5;

  Tensor<float, 4, RowMajor> input_backward(input_cols, input_rows,
                                            input_planes, input_depth);
  Tensor<float, 5, RowMajor> kernel(patch_cols, patch_rows, patch_planes,
                                    input_depth, output_depth);
  Tensor<float, 4, RowMajor> output_backward(output_cols, output_rows,
                                             output_planes, output_depth);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  kernel = kernel.constant(2.0f) + kernel.random();
  input_backward.setRandom();

  input_backward = CuboidConvolutionBackwardInput(
      kernel, output_backward, input_planes, input_rows, input_cols);

  EXPECT_EQ(input_backward.dimension(0), input_cols);
  EXPECT_EQ(input_backward.dimension(1), input_rows);
  EXPECT_EQ(input_backward.dimension(2), input_planes);
  EXPECT_EQ(input_backward.dimension(3), input_depth);

  for (int id = 0; id < input_depth; ++id) {
    for (int i = 0; i < input_planes; ++i) {
      for (int j = 0; j < input_rows; ++j) {
        for (int k = 0; k < input_cols; ++k) {
          float expected = 0.0f;
          for (int c = 0; c < patch_cols; ++c) {
            for (int r = 0; r < patch_rows; ++r) {
              for (int p = 0; p < patch_planes; ++p) {
                for (int od = 0; od < output_depth; ++od) {
                  int output_j = j - r;
                  int output_k = k - c;
                  int output_i = i - p;
                  if (output_i >= 0 && output_i < output_planes &&
                      output_j >= 0 && output_j < output_rows &&
                      output_k >= 0 && output_k < output_cols) {
                    expected +=
                        output_backward(output_k, output_j, output_i, od) *
                        kernel(c, r, p, id, od);
                  }
                }
              }
            }
          }
          EigenApprox(input_backward(k, j, i, id), expected);
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_simple_cuboid_convolution_backward_input_same) {
  const int input_depth = 2;
  const int input_planes = 5;
  const int input_rows = 3;
  const int input_cols = 4;
  const int patch_rows = 3;
  const int patch_cols = 2;
  const int patch_planes = 4;
  const int output_rows = input_rows;
  const int output_cols = input_cols;
  const int output_planes = input_planes;
  const int output_depth = 5;

  Tensor<float, 4> input_backward(input_depth, input_planes, input_rows,
                                  input_cols);
  Tensor<float, 5> kernel(output_depth, input_depth, patch_planes, patch_rows,
                          patch_cols);
  Tensor<float, 4> output_backward(output_depth, output_planes, output_rows,
                                   output_cols);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  kernel = kernel.constant(2.0f) + kernel.random();
  input_backward.setRandom();

  input_backward = CuboidConvolutionBackwardInput(
      kernel, output_backward, input_planes, input_rows, input_cols);

  EXPECT_EQ(input_backward.dimension(3), input_cols);
  EXPECT_EQ(input_backward.dimension(2), input_rows);
  EXPECT_EQ(input_backward.dimension(1), input_planes);
  EXPECT_EQ(input_backward.dimension(0), input_depth);

  const int dz = patch_planes - 1;
  const int dy = patch_rows - 1;
  const int dx = patch_cols - 1;

  const int forward_pad_x = dx / 2;
  const int forward_pad_y = dy / 2;
  const int forward_pad_z = dz / 2;

  for (int id = 0; id < input_depth; ++id) {
    for (int i = 0; i < input_planes; ++i) {
      for (int j = 0; j < input_rows; ++j) {
        for (int k = 0; k < input_cols; ++k) {
          float expected = 0.0f;
          for (int c = 0; c < patch_cols; ++c) {
            for (int r = 0; r < patch_rows; ++r) {
              for (int p = 0; p < patch_planes; ++p) {
                for (int od = 0; od < output_depth; ++od) {
                  int output_i = i - p + forward_pad_z;
                  int output_j = j - r + forward_pad_y;
                  int output_k = k - c + forward_pad_x;
                  if (output_i >= 0 && output_i < output_planes &&
                      output_j >= 0 && output_j < output_rows &&
                      output_k >= 0 && output_k < output_cols) {
                    expected +=
                        output_backward(od, output_i, output_j, output_k) *
                        kernel(od, id, p, r, c);
                  }
                }
              }
            }
          }
          EigenApprox(input_backward(id, i, j, k), expected);
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_simple_cuboid_convolution_backward_input_same_row_major) {
  const int input_depth = 2;
  const int input_planes = 5;
  const int input_rows = 3;
  const int input_cols = 4;
  const int patch_rows = 2;
  const int patch_cols = 3;
  const int patch_planes = 4;
  const int output_rows = input_rows;
  const int output_cols = input_cols;
  const int output_planes = input_planes;
  const int output_depth = 5;

  Tensor<float, 4, RowMajor> input_backward(input_cols, input_rows,
                                            input_planes, input_depth);
  Tensor<float, 5, RowMajor> kernel(patch_cols, patch_rows, patch_planes,
                                    input_depth, output_depth);
  Tensor<float, 4, RowMajor> output_backward(output_cols, output_rows,
                                             output_planes, output_depth);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  kernel = kernel.constant(2.0f) + kernel.random();
  input_backward.setRandom();

  input_backward = CuboidConvolutionBackwardInput(
      kernel, output_backward, input_planes, input_rows, input_cols);

  EXPECT_EQ(input_backward.dimension(0), input_cols);
  EXPECT_EQ(input_backward.dimension(1), input_rows);
  EXPECT_EQ(input_backward.dimension(2), input_planes);
  EXPECT_EQ(input_backward.dimension(3), input_depth);

  const int dz = patch_planes - 1;
  const int dy = patch_rows - 1;
  const int dx = patch_cols - 1;

  const int forward_pad_x = dx / 2;
  const int forward_pad_y = dy / 2;
  const int forward_pad_z = dz / 2;

  for (int id = 0; id < input_depth; ++id) {
    for (int i = 0; i < input_planes; ++i) {
      for (int j = 0; j < input_rows; ++j) {
        for (int k = 0; k < input_cols; ++k) {
          float expected = 0.0f;
          for (int c = 0; c < patch_cols; ++c) {
            for (int r = 0; r < patch_rows; ++r) {
              for (int p = 0; p < patch_planes; ++p) {
                for (int od = 0; od < output_depth; ++od) {
                  int output_i = i - p + forward_pad_z;
                  int output_j = j - r + forward_pad_y;
                  int output_k = k - c + forward_pad_x;
                  if (output_i >= 0 && output_i < output_planes &&
                      output_j >= 0 && output_j < output_rows &&
                      output_k >= 0 && output_k < output_cols) {
                    expected +=
                        output_backward(output_k, output_j, output_i, od) *
                        kernel(c, r, p, id, od);
                  }
                }
              }
            }
          }
          EigenApprox(input_backward(k, j, i, id), expected);
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_batched_cuboid_convolution_backward_input_valid) {
  const int num_batches = 13;
  const int input_depth = 2;
  const int input_planes = 5;
  const int input_rows = 3;
  const int input_cols = 4;
  const int patch_rows = 2;
  const int patch_cols = 2;
  const int patch_planes = 2;
  const int output_rows = input_rows - patch_rows + 1;
  const int output_cols = input_cols - patch_cols + 1;
  const int output_planes = input_planes - patch_planes + 1;
  const int output_depth = 5;

  Tensor<float, 5> input_backward(input_depth, input_planes, input_rows,
                                  input_cols, num_batches);
  Tensor<float, 5> kernel(output_depth, input_depth, patch_planes, patch_rows,
                          patch_cols);
  Tensor<float, 5> output_backward(output_depth, output_planes, output_rows,
                                   output_cols, num_batches);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  kernel = kernel.constant(2.0f) + kernel.random();
  input_backward.setRandom();

  input_backward = CuboidConvolutionBackwardInput(
      kernel, output_backward, input_planes, input_rows, input_cols);

  EXPECT_EQ(input_backward.dimension(4), num_batches);
  EXPECT_EQ(input_backward.dimension(3), input_cols);
  EXPECT_EQ(input_backward.dimension(2), input_rows);
  EXPECT_EQ(input_backward.dimension(1), input_planes);
  EXPECT_EQ(input_backward.dimension(0), input_depth);

  for (int b = 0; b < num_batches; ++b) {
    for (int id = 0; id < input_depth; ++id) {
      for (int i = 0; i < input_planes; ++i) {
        for (int j = 0; j < input_rows; ++j) {
          for (int k = 0; k < input_cols; ++k) {
            float expected = 0.0f;
            for (int c = 0; c < patch_cols; ++c) {
              for (int r = 0; r < patch_rows; ++r) {
                for (int p = 0; p < patch_planes; ++p) {
                  for (int od = 0; od < output_depth; ++od) {
                    int output_i = i - p;
                    int output_j = j - r;
                    int output_k = k - c;
                    if (output_i >= 0 && output_i < output_planes &&
                        output_j >= 0 && output_j < output_rows &&
                        output_k >= 0 && output_k < output_cols) {
                      expected +=
                          output_backward(od, output_i, output_j, output_k, b) *
                          kernel(od, id, p, r, c);
                    }
                  }
                }
              }
            }
            EigenApprox(input_backward(id, i, j, k, b), expected);
          }
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_batched_cuboid_convolution_backward_input_valid_row_major) {
  const int num_batches = 13;
  const int input_depth = 2;
  const int input_planes = 5;
  const int input_rows = 3;
  const int input_cols = 4;
  const int patch_rows = 2;
  const int patch_cols = 2;
  const int patch_planes = 2;
  const int output_rows = input_rows - patch_rows + 1;
  const int output_cols = input_cols - patch_cols + 1;
  const int output_planes = input_planes - patch_planes + 1;
  const int output_depth = 5;

  Tensor<float, 5, RowMajor> input_backward(num_batches, input_cols, input_rows,
                                            input_planes, input_depth);
  Tensor<float, 5, RowMajor> kernel(patch_cols, patch_rows, patch_planes,
                                    input_depth, output_depth);
  Tensor<float, 5, RowMajor> output_backward(
      num_batches, output_cols, output_rows, output_planes, output_depth);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  kernel = kernel.constant(2.0f) + kernel.random();
  input_backward.setRandom();

  input_backward = CuboidConvolutionBackwardInput(
      kernel, output_backward, input_planes, input_rows, input_cols);

  EXPECT_EQ(input_backward.dimension(0), num_batches);
  EXPECT_EQ(input_backward.dimension(1), input_cols);
  EXPECT_EQ(input_backward.dimension(2), input_rows);
  EXPECT_EQ(input_backward.dimension(3), input_planes);
  EXPECT_EQ(input_backward.dimension(4), input_depth);

  for (int b = 0; b < num_batches; ++b) {
    for (int id = 0; id < input_depth; ++id) {
      for (int i = 0; i < input_planes; ++i) {
        for (int j = 0; j < input_rows; ++j) {
          for (int k = 0; k < input_cols; ++k) {
            float expected = 0.0f;
            for (int c = 0; c < patch_cols; ++c) {
              for (int r = 0; r < patch_rows; ++r) {
                for (int p = 0; p < patch_planes; ++p) {
                  for (int od = 0; od < output_depth; ++od) {
                    int output_i = i - p;
                    int output_j = j - r;
                    int output_k = k - c;
                    if (output_i >= 0 && output_i < output_planes &&
                        output_j >= 0 && output_j < output_rows &&
                        output_k >= 0 && output_k < output_cols) {
                      expected +=
                          output_backward(b, output_k, output_j, output_i, od) *
                          kernel(c, r, p, id, od);
                    }
                  }
                }
              }
            }
            EigenApprox(input_backward(b, k, j, i, id), expected);
          }
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_simple_cuboid_convolution_backward_kernel_valid) {
  const int input_depth = 2;
  const int input_planes = 5;
  const int input_rows = 3;
  const int input_cols = 4;
  const int output_depth = 5;
  const int patch_rows = 2;
  const int patch_cols = 2;
  const int patch_planes = 3;
  const int output_rows = input_rows - patch_rows + 1;
  const int output_cols = input_cols - patch_cols + 1;
  const int output_planes = input_planes - patch_planes + 1;

  // TODO(ezhulenev): Support backward kernel convolution without batch
  // dimension.
  Tensor<float, 5> input(input_depth, input_planes, input_rows, input_cols,
                         /*num_batches*/ 1);
  Tensor<float, 5> kernel(output_depth, input_depth, patch_planes, patch_rows,
                          patch_cols);
  Tensor<float, 5> output_backward(output_depth, output_planes, output_rows,
                                   output_cols, /*num_batches*/ 1);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  input = input.constant(2.0f) + input.random();
  kernel.setRandom();

  kernel = CuboidConvolutionBackwardKernel(input, output_backward, patch_planes,
                                           patch_rows, patch_cols, 1, 1, 1);

  EXPECT_EQ(kernel.dimension(0), output_depth);
  EXPECT_EQ(kernel.dimension(1), input_depth);
  EXPECT_EQ(kernel.dimension(2), patch_planes);
  EXPECT_EQ(kernel.dimension(3), patch_rows);
  EXPECT_EQ(kernel.dimension(4), patch_cols);

  for (int od = 0; od < output_depth; ++od) {
    for (int id = 0; id < input_depth; ++id) {
      for (int p = 0; p < patch_planes; ++p) {
        for (int r = 0; r < patch_rows; ++r) {
          for (int c = 0; c < patch_cols; ++c) {
            float expected = 0.0f;
            for (int i = 0; i < input_planes; ++i) {
              for (int j = 0; j < input_rows; ++j) {
                for (int k = 0; k < input_cols; ++k) {
                  int output_j = j - r;
                  int output_k = k - c;
                  int output_i = i - p;
                  if (output_i >= 0 && output_i < output_planes &&
                      output_j >= 0 && output_j < output_rows &&
                      output_k >= 0 && output_k < output_cols) {
                    expected += input(id, i, j, k, /*batch*/ 0) *
                                output_backward(od, output_i, output_j,
                                                output_k, /*batch*/ 0);
                  }
                }
              }
            }
            EigenApprox(kernel(od, id, p, r, c), expected);
          }
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_simple_cuboid_convolution_backward_kernel_valid_row_major) {
  const int input_depth = 2;
  const int input_planes = 5;
  const int input_rows = 3;
  const int input_cols = 4;
  const int output_depth = 5;
  const int patch_rows = 2;
  const int patch_cols = 2;
  const int patch_planes = 3;
  const int output_rows = input_rows - patch_rows + 1;
  const int output_cols = input_cols - patch_cols + 1;
  const int output_planes = input_planes - patch_planes + 1;

  // TODO(ezhulenev): Support backward kernel convolution without batch
  // dimension.
  Tensor<float, 5, RowMajor> input(/*num_batches*/ 1, input_cols, input_rows,
                                   input_planes, input_depth);
  Tensor<float, 5, RowMajor> kernel(patch_cols, patch_rows, patch_planes,
                                    input_depth, output_depth);
  Tensor<float, 5, RowMajor> output_backward(
      /*num_batches*/ 1, output_cols, output_rows, output_planes, output_depth);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  input = input.constant(2.0f) + input.random();
  kernel.setRandom();

  kernel = CuboidConvolutionBackwardKernel(input, output_backward, patch_planes,
                                           patch_rows, patch_cols, 1, 1, 1);

  EXPECT_EQ(kernel.dimension(4), output_depth);
  EXPECT_EQ(kernel.dimension(3), input_depth);
  EXPECT_EQ(kernel.dimension(2), patch_planes);
  EXPECT_EQ(kernel.dimension(1), patch_rows);
  EXPECT_EQ(kernel.dimension(0), patch_cols);

  for (int od = 0; od < output_depth; ++od) {
    for (int id = 0; id < input_depth; ++id) {
      for (int p = 0; p < patch_planes; ++p) {
        for (int r = 0; r < patch_rows; ++r) {
          for (int c = 0; c < patch_cols; ++c) {
            float expected = 0.0f;
            for (int i = 0; i < input_planes; ++i) {
              for (int j = 0; j < input_rows; ++j) {
                for (int k = 0; k < input_cols; ++k) {
                  int output_j = j - r;
                  int output_k = k - c;
                  int output_i = i - p;
                  if (output_i >= 0 && output_i < output_planes &&
                      output_j >= 0 && output_j < output_rows &&
                      output_k >= 0 && output_k < output_cols) {
                    expected += input(/*batch*/ 0, k, j, i, id) *
                                output_backward(/*batch*/ 0, output_k, output_j,
                                                output_i, od);
                  }
                }
              }
            }
            EigenApprox(kernel(c, r, p, id, od), expected);
          }
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_batched_cuboid_convolution_backward_kernel_valid) {
  const int num_batches = 13;
  const int input_depth = 2;
  const int input_planes = 5;
  const int input_rows = 7;
  const int input_cols = 9;
  const int output_depth = 3;
  const int patch_rows = 5;
  const int patch_cols = 5;
  const int patch_planes = 3;
  const int output_rows = input_rows - patch_rows + 1;
  const int output_cols = input_cols - patch_cols + 1;
  const int output_planes = input_planes - patch_planes + 1;

  Tensor<float, 5> input(input_depth, input_planes, input_rows, input_cols,
                         num_batches);
  Tensor<float, 5> kernel_backward(output_depth, input_depth, patch_planes,
                                   patch_rows, patch_cols);
  Tensor<float, 5> output_backward(output_depth, output_planes, output_rows,
                                   output_cols, num_batches);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  input = input.constant(2.0f) + input.random();
  kernel_backward.setRandom();

  kernel_backward = CuboidConvolutionBackwardKernel(
      input, output_backward, patch_planes, patch_rows, patch_cols, 1, 1, 1);

  EXPECT_EQ(kernel_backward.dimension(0), output_depth);
  EXPECT_EQ(kernel_backward.dimension(1), input_depth);
  EXPECT_EQ(kernel_backward.dimension(2), patch_planes);
  EXPECT_EQ(kernel_backward.dimension(3), patch_rows);
  EXPECT_EQ(kernel_backward.dimension(4), patch_cols);

  for (int od = 0; od < output_depth; ++od) {
    for (int id = 0; id < input_depth; ++id) {
      for (int p = 0; p < patch_planes; ++p) {
        for (int c = 0; c < patch_cols; ++c) {
          for (int r = 0; r < patch_rows; ++r) {
            float expected = 0.0f;
            for (int b = 0; b < num_batches; ++b) {
              for (int i = 0; i < input_planes; ++i) {
                for (int j = 0; j < input_rows; ++j) {
                  for (int k = 0; k < input_cols; ++k) {
                    int output_j = j - r;
                    int output_k = k - c;
                    int output_i = i - p;
                    if (output_i >= 0 && output_i < output_planes &&
                        output_j >= 0 && output_j < output_rows &&
                        output_k >= 0 && output_k < output_cols) {
                      expected +=
                          input(id, i, j, k, b) *
                          output_backward(od, output_i, output_j, output_k, b);
                    }
                  }
                }
              }
            }
            EigenApprox(kernel_backward(od, id, p, r, c), expected);
          }
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_batched_cuboid_convolution_backward_kernel_valid_row_major) {
  const int num_batches = 13;
  const int input_depth = 2;
  const int input_planes = 5;
  const int input_rows = 7;
  const int input_cols = 9;
  const int output_depth = 3;
  const int patch_rows = 5;
  const int patch_cols = 5;
  const int patch_planes = 3;
  const int output_rows = input_rows - patch_rows + 1;
  const int output_cols = input_cols - patch_cols + 1;
  const int output_planes = input_planes - patch_planes + 1;

  Tensor<float, 5, RowMajor> input(num_batches, input_cols, input_rows,
                                   input_planes, input_depth);
  Tensor<float, 5, RowMajor> kernel_backward(
      patch_cols, patch_rows, patch_planes, input_depth, output_depth);
  Tensor<float, 5, RowMajor> output_backward(
      num_batches, output_cols, output_rows, output_planes, output_depth);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  input = input.constant(2.0f) + input.random();
  kernel_backward.setRandom();

  kernel_backward = CuboidConvolutionBackwardKernel(
      input, output_backward, patch_planes, patch_rows, patch_cols, 1, 1, 1);

  EXPECT_EQ(kernel_backward.dimension(4), output_depth);
  EXPECT_EQ(kernel_backward.dimension(3), input_depth);
  EXPECT_EQ(kernel_backward.dimension(2), patch_planes);
  EXPECT_EQ(kernel_backward.dimension(1), patch_rows);
  EXPECT_EQ(kernel_backward.dimension(0), patch_cols);

  for (int od = 0; od < output_depth; ++od) {
    for (int id = 0; id < input_depth; ++id) {
      for (int p = 0; p < patch_planes; ++p) {
        for (int c = 0; c < patch_cols; ++c) {
          for (int r = 0; r < patch_rows; ++r) {
            float expected = 0.0f;
            for (int b = 0; b < num_batches; ++b) {
              for (int i = 0; i < input_planes; ++i) {
                for (int j = 0; j < input_rows; ++j) {
                  for (int k = 0; k < input_cols; ++k) {
                    int output_j = j - r;
                    int output_k = k - c;
                    int output_i = i - p;
                    if (output_i >= 0 && output_i < output_planes &&
                        output_j >= 0 && output_j < output_rows &&
                        output_k >= 0 && output_k < output_cols) {
                      expected +=
                          input(b, k, j, i, id) *
                          output_backward(b, output_k, output_j, output_i, od);
                    }
                  }
                }
              }
            }
            EigenApprox(kernel_backward(c, r, p, id, od), expected);
          }
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_batched_strided_cuboid_convolution_backward_kernel_valid) {
  const int num_batches = 13;
  const int input_depth = 2;
  const int input_planes = 8;
  const int input_rows = 7;
  const int input_cols = 9;
  const int output_depth = 3;
  const int patch_planes = 3;
  const int patch_rows = 3;
  const int patch_cols = 2;

  const int stride_planes = 2;
  const int stride_cols = 3;
  const int stride_rows = 1;

  const int output_rows = ceil_div(input_rows - patch_rows + 1, stride_rows);
  const int output_cols = ceil_div(input_cols - patch_cols + 1, stride_cols);
  const int output_planes =
      ceil_div(input_planes - patch_planes + 1, stride_planes);

  Tensor<float, 5> input(input_depth, input_planes, input_rows, input_cols,
                         num_batches);
  Tensor<float, 5> kernel_backward(output_depth, input_depth, patch_planes,
                                   patch_rows, patch_cols);
  Tensor<float, 5> output_backward(output_depth, output_planes, output_rows,
                                   output_cols, num_batches);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  input = input.constant(2.0f) + input.random();
  kernel_backward.setRandom();

  kernel_backward = CuboidConvolutionBackwardKernel(
      input, output_backward, patch_planes, patch_rows, patch_cols,
      stride_planes, stride_rows, stride_cols);

  EXPECT_EQ(kernel_backward.dimension(0), output_depth);
  EXPECT_EQ(kernel_backward.dimension(1), input_depth);
  EXPECT_EQ(kernel_backward.dimension(2), patch_planes);
  EXPECT_EQ(kernel_backward.dimension(3), patch_rows);
  EXPECT_EQ(kernel_backward.dimension(4), patch_cols);

  for (int od = 0; od < output_depth; ++od) {
    for (int id = 0; id < input_depth; ++id) {
      for (int p = 0; p < patch_planes; ++p) {
        for (int c = 0; c < patch_cols; ++c) {
          for (int r = 0; r < patch_rows; ++r) {
            float expected = 0.0f;
            for (int b = 0; b < num_batches; ++b) {
              for (int i = 0; i < input_planes; ++i) {
                for (int j = 0; j < input_rows; ++j) {
                  for (int k = 0; k < input_cols; ++k) {
                    int output_j = j - r;
                    int output_k = k - c;
                    int output_i = i - p;
                    if (output_i >= 0 &&
                        output_i / stride_planes < output_planes &&
                        output_j >= 0 && output_j / stride_rows < output_rows &&
                        output_k >= 0 && output_k / stride_cols < output_cols &&
                        output_i % stride_planes == 0 &&
                        output_j % stride_rows == 0 &&
                        output_k % stride_cols == 0) {
                      expected += input(id, i, j, k, b) *
                                  output_backward(od, output_i / stride_planes,
                                                  output_j / stride_rows,
                                                  output_k / stride_cols, b);
                    }
                  }
                }
              }
            }
            EigenApprox(kernel_backward(od, id, p, r, c), expected);
          }
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_batched_strided_cuboid_convolution_backward_kernel_valid_row_major) {
  const int num_batches = 13;
  const int input_depth = 2;
  const int input_planes = 8;
  const int input_rows = 7;
  const int input_cols = 9;
  const int output_depth = 3;
  const int patch_planes = 3;
  const int patch_rows = 3;
  const int patch_cols = 2;

  const int stride_planes = 2;
  const int stride_cols = 3;
  const int stride_rows = 1;

  const int output_rows = ceil_div(input_rows - patch_rows + 1, stride_rows);
  const int output_cols = ceil_div(input_cols - patch_cols + 1, stride_cols);
  const int output_planes =
      ceil_div(input_planes - patch_planes + 1, stride_planes);

  Tensor<float, 5, RowMajor> input(num_batches, input_cols, input_rows,
                                   input_planes, input_depth);
  Tensor<float, 5, RowMajor> kernel_backward(
      patch_cols, patch_rows, patch_planes, input_depth, output_depth);
  Tensor<float, 5, RowMajor> output_backward(
      num_batches, output_cols, output_rows, output_planes, output_depth);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  input = input.constant(2.0f) + input.random();
  kernel_backward.setRandom();

  kernel_backward = CuboidConvolutionBackwardKernel(
      input, output_backward, patch_planes, patch_rows, patch_cols,
      stride_planes, stride_rows, stride_cols);

  EXPECT_EQ(kernel_backward.dimension(4), output_depth);
  EXPECT_EQ(kernel_backward.dimension(3), input_depth);
  EXPECT_EQ(kernel_backward.dimension(2), patch_planes);
  EXPECT_EQ(kernel_backward.dimension(1), patch_rows);
  EXPECT_EQ(kernel_backward.dimension(0), patch_cols);

  for (int od = 0; od < output_depth; ++od) {
    for (int id = 0; id < input_depth; ++id) {
      for (int p = 0; p < patch_planes; ++p) {
        for (int c = 0; c < patch_cols; ++c) {
          for (int r = 0; r < patch_rows; ++r) {
            float expected = 0.0f;
            for (int b = 0; b < num_batches; ++b) {
              for (int i = 0; i < input_planes; ++i) {
                for (int j = 0; j < input_rows; ++j) {
                  for (int k = 0; k < input_cols; ++k) {
                    int output_j = j - r;
                    int output_k = k - c;
                    int output_i = i - p;
                    if (output_i >= 0 &&
                        output_i / stride_planes < output_planes &&
                        output_j >= 0 && output_j / stride_rows < output_rows &&
                        output_k >= 0 && output_k / stride_cols < output_cols &&
                        output_i % stride_planes == 0 &&
                        output_j % stride_rows == 0 &&
                        output_k % stride_cols == 0) {
                      expected += input(b, k, j, i, id) *
                                  output_backward(b, output_k / stride_cols,
                                                  output_j / stride_rows,
                                                  output_i / stride_planes, od);
                    }
                  }
                }
              }
            }
            EigenApprox(kernel_backward(c, r, p, id, od), expected);
          }
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_batched_strided_cuboid_convolution_backward_input_valid) {
  const int num_batches = 13;
  const int input_depth = 2;
  const int input_planes = 14;
  const int input_rows = 13;
  const int input_cols = 15;
  const int patch_rows = 3;
  const int patch_cols = 2;
  const int patch_planes = 4;
  const int stride_rows = 3;
  const int stride_cols = 2;
  const int stride_planes = 3;
  const int output_rows = ceil_div(input_rows - patch_rows + 1, stride_rows);
  const int output_cols = ceil_div(input_cols - patch_cols + 1, stride_cols);
  const int output_planes =
      ceil_div(input_planes - patch_planes + 1, stride_planes);
  const int output_depth = 5;

  Tensor<float, 5> input_backward(input_depth, input_planes, input_rows,
                                  input_cols, num_batches);
  Tensor<float, 5> kernel(output_depth, input_depth, patch_planes, patch_rows,
                          patch_cols);
  Tensor<float, 5> output_backward(output_depth, output_planes, output_rows,
                                   output_cols, num_batches);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  kernel = kernel.constant(2.0f) + kernel.random();
  input_backward.setRandom();

  input_backward = CuboidConvolutionBackwardInput(
      kernel, output_backward, input_planes, input_rows, input_cols,
      stride_planes, stride_rows, stride_cols);

  EXPECT_EQ(input_backward.dimension(4), num_batches);
  EXPECT_EQ(input_backward.dimension(3), input_cols);
  EXPECT_EQ(input_backward.dimension(2), input_rows);
  EXPECT_EQ(input_backward.dimension(1), input_planes);
  EXPECT_EQ(input_backward.dimension(0), input_depth);

  for (int b = 0; b < num_batches; ++b) {
    for (int id = 0; id < input_depth; ++id) {
      for (int i = 0; i < input_planes; ++i) {
        for (int j = 0; j < input_rows; ++j) {
          for (int k = 0; k < input_cols; ++k) {
            float expected = 0.0f;
            for (int c = 0; c < patch_cols; ++c) {
              for (int r = 0; r < patch_rows; ++r) {
                for (int p = 0; p < patch_planes; ++p) {
                  for (int od = 0; od < output_depth; ++od) {
                    int output_j = j - r;
                    int output_k = k - c;
                    int output_i = i - p;
                    if (output_i >= 0 &&
                        output_i / stride_planes < output_planes &&
                        output_j >= 0 && output_j / stride_rows < output_rows &&
                        output_k >= 0 && output_k / stride_cols < output_cols &&
                        output_i % stride_planes == 0 &&
                        output_j % stride_rows == 0 &&
                        output_k % stride_cols == 0) {
                      expected += output_backward(od, output_i / stride_planes,
                                                  output_j / stride_rows,
                                                  output_k / stride_cols, b) *
                                  kernel(od, id, p, r, c);
                    }
                  }
                }
              }
            }
            EigenApprox(input_backward(id, i, j, k, b), expected);
          }
        }
      }
    }
  }
}

TEST(EigenBackwardSpatialConvolutionsTest,
     test_batched_strided_cuboid_convolution_backward_input_valid_row_major) {
  const int num_batches = 13;
  const int input_depth = 2;
  const int input_planes = 14;
  const int input_rows = 13;
  const int input_cols = 15;
  const int patch_rows = 3;
  const int patch_cols = 2;
  const int patch_planes = 4;
  const int stride_rows = 3;
  const int stride_cols = 2;
  const int stride_planes = 3;
  const int output_rows = ceil_div(input_rows - patch_rows + 1, stride_rows);
  const int output_cols = ceil_div(input_cols - patch_cols + 1, stride_cols);
  const int output_planes =
      ceil_div(input_planes - patch_planes + 1, stride_planes);
  const int output_depth = 5;

  Tensor<float, 5, RowMajor> input_backward(num_batches, input_cols, input_rows,
                                            input_planes, input_depth);
  Tensor<float, 5, RowMajor> kernel(patch_cols, patch_rows, patch_planes,
                                    input_depth, output_depth);
  Tensor<float, 5, RowMajor> output_backward(
      num_batches, output_cols, output_rows, output_planes, output_depth);

  output_backward = output_backward.constant(11.0f) + output_backward.random();
  kernel = kernel.constant(2.0f) + kernel.random();
  input_backward.setRandom();

  input_backward = CuboidConvolutionBackwardInput(
      kernel, output_backward, input_planes, input_rows, input_cols,
      stride_planes, stride_rows, stride_cols);

  EXPECT_EQ(input_backward.dimension(0), num_batches);
  EXPECT_EQ(input_backward.dimension(1), input_cols);
  EXPECT_EQ(input_backward.dimension(2), input_rows);
  EXPECT_EQ(input_backward.dimension(3), input_planes);
  EXPECT_EQ(input_backward.dimension(4), input_depth);

  for (int b = 0; b < num_batches; ++b) {
    for (int id = 0; id < input_depth; ++id) {
      for (int i = 0; i < input_planes; ++i) {
        for (int j = 0; j < input_rows; ++j) {
          for (int k = 0; k < input_cols; ++k) {
            float expected = 0.0f;
            for (int c = 0; c < patch_cols; ++c) {
              for (int r = 0; r < patch_rows; ++r) {
                for (int p = 0; p < patch_planes; ++p) {
                  for (int od = 0; od < output_depth; ++od) {
                    int output_j = j - r;
                    int output_k = k - c;
                    int output_i = i - p;
                    if (output_i >= 0 &&
                        output_i / stride_planes < output_planes &&
                        output_j >= 0 && output_j / stride_rows < output_rows &&
                        output_k >= 0 && output_k / stride_cols < output_cols &&
                        output_i % stride_planes == 0 &&
                        output_j % stride_rows == 0 &&
                        output_k % stride_cols == 0) {
                      expected +=
                          output_backward(b, output_k / stride_cols,
                                          output_j / stride_rows,
                                          output_i / stride_planes, od) *
                          kernel(c, r, p, id, od);
                    }
                  }
                }
              }
            }
            EigenApprox(input_backward(b, k, j, i, id), expected);
          }
        }
      }
    }
  }
}

}  // namespace Eigen
