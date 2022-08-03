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

// Types to express dimensionality of a kernel launch. Blocks and threads
// are (up to) 3-dimensional.
//
// A thread is conceptually like a SIMD lane. Some number, typically 32
// (though that fact should not be relied on) SIMD lanes are tied together with
// a single PC in a unit called a warp. There is a maximum number of threads
// that can execute in a shared-context entity called a block. Presently, that
// number is 1024 -- again, something that should not be relied on from this
// comment, but checked via stream_executor::DeviceDescription.
//
// For additional information, see
// http://docs.nvidia.com/cuda/kepler-tuning-guide/#device-utilization-and-occupancy
//
// Because of that modest thread-per-block limit, a kernel can be launched with
// multiple blocks. Each block is indivisibly scheduled onto a single core.
// Blocks can also be used in a multi-dimensional configuration, and the block
// count has much less modest limits -- typically they're similar to the maximum
// amount of addressable memory.

#ifndef TENSORFLOW_STREAM_EXECUTOR_LAUNCH_DIM_H_
#define TENSORFLOW_STREAM_EXECUTOR_LAUNCH_DIM_H_

#include "tensorflow/compiler/xla/stream_executor/launch_dim.h"

#endif  // TENSORFLOW_STREAM_EXECUTOR_LAUNCH_DIM_H_
