/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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

// Generated by the tf_library build rule.  DO NOT EDIT!
//
// This file contains a test and benchmark for the function generated by
// tfcompile.  All tokens of the form `{{TFCOMPILE_*}}` must be rewritten to
// real values before this file can be compiled.
//
//    TFCOMPILE_HEADER    : Path to the header file generated by tfcompile.
//    TFCOMPILE_CPP_CLASS : Name of the C++ class generated by tfcompile.
//    TFCOMPILE_NAME      : Name for tests and benchmarks.
//
// The tf_library bazel macro in tfcompile.bzl performs the token rewriting, and
// generates a cc_test rule for you.

// These macros must be defined before eigen files are included.
#define EIGEN_USE_THREADS
#define EIGEN_USE_CUSTOM_THREAD_POOL

// clang-format off
#include "{{TFCOMPILE_HEADER}}"  // NOLINT(whitespace/braces)
// clang-format on

#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"
#include "tensorflow/core/platform/byte_order.h"
#include "tensorflow/core/platform/cpu_info.h"
#include "tensorflow/core/platform/test.h"
#include "tensorflow/core/platform/test_benchmark.h"

// Macros that expand to tokens based on the entry point name.
// clang-format off
#define CPP_CLASS {{TFCOMPILE_CPP_CLASS}}  // NOLINT(whitespace/braces)
#define TEST_NAME {{TFCOMPILE_NAME}}Test   // NOLINT(whitespace/braces)
#define BM_NAME   BM_{{TFCOMPILE_NAME}}    // NOLINT(whitespace/braces)
// clang-format on

namespace tensorflow {
namespace tfcompile {
namespace {

void zero_buffers(XlaCompiledCpuFunction* computation) {
  for (int i = 0; i < computation->num_args(); ++i) {
    memset(computation->arg_data(i), 0, computation->arg_size(i));
  }
}

// Trivial test that runs the generated function to ensure it doesn't crash.
TEST(TEST_NAME, NoCrash) {
  Eigen::ThreadPool pool(port::NumSchedulableCPUs());
  Eigen::ThreadPoolDevice device(&pool, pool.NumThreads());

  CPP_CLASS computation;
  computation.set_thread_pool(&device);
  zero_buffers(&computation);

  EXPECT_TRUE(computation.Run());
}

// Simple benchmark that repeatedly runs the generated function.
void BM_NAME(int iters) {
  testing::StopTiming();

  Eigen::ThreadPool pool(port::NumSchedulableCPUs());
  Eigen::ThreadPoolDevice device(&pool, pool.NumThreads());

  CPP_CLASS computation;
  computation.set_thread_pool(&device);
  zero_buffers(&computation);

  testing::StartTiming();
  while (--iters) {
    computation.Run();
  }
  testing::StopTiming();
}
BENCHMARK(BM_NAME);

}  // namespace
}  // namespace tfcompile
}  // namespace tensorflow
