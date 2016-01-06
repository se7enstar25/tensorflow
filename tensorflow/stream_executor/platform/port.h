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

// IWYU pragma: private, include "perftools/gputools/executor/stream_executor.h"

#ifndef TENSORFLOW_STREAM_EXECUTOR_PLATFORM_PORT_H_
#define TENSORFLOW_STREAM_EXECUTOR_PLATFORM_PORT_H_

#include "tensorflow/core/platform/macros.h"
#include "tensorflow/core/platform/port.h"

namespace perftools {
namespace gputools {

using tensorflow::int8;
using tensorflow::int16;
using tensorflow::int32;
using tensorflow::int64;

using tensorflow::uint8;
using tensorflow::uint16;
using tensorflow::uint32;
using tensorflow::uint64;

#if !defined(PLATFORM_GOOGLE)
using std::string;
#endif

#if !defined(COMPILER_MSVC)
#define ARRAYSIZE(a)              \
    ((sizeof(a) / sizeof(*(a))) / \
    static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))
#endif

using tensorflow::LinkerInitialized;
using tensorflow::LINKER_INITIALIZED;

#define SE_FALLTHROUGH_INTENDED TF_FALLTHROUGH_INTENDED

}  // namespace gputools
}  // namespace perftools

#define SE_DISALLOW_COPY_AND_ASSIGN TF_DISALLOW_COPY_AND_ASSIGN
#define SE_MUST_USE_RESULT TF_MUST_USE_RESULT

#endif  // TENSORFLOW_STREAM_EXECUTOR_PLATFORM_PORT_H_
