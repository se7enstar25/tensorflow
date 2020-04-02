/* Copyright 2019 Google LLC. All Rights Reserved.

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

#include "tensorflow/lite/experimental/ruy/ruy/have_built_path_for.h"
#include "tensorflow/lite/experimental/ruy/ruy/opt_set.h"

namespace ruy {

#if RUY_PLATFORM(X86)
// IMPORTANT:
// These patterns must match those in the pack and kernel cc files.
#if !(RUY_PLATFORM(SSE42) && RUY_OPT_ENABLED(RUY_OPT_ASM))

bool HaveBuiltPathForSse42() { return false; }

#else  // RUY_PLATFORM(SSE42) && RUY_OPT_ENABLED(RUY_OPT_ASM)

// TODO(b/147376783): SSE 4.2 and AVX-VNNI support is incomplete / placeholder.
// Optimization is not finished. In particular the dimensions of the kernel
// blocks can be changed as desired.
//
bool HaveBuiltPathForSse42() { return true; }

#endif  // RUY_PLATFORM(SSE42) && RUY_OPT_ENABLED(RUY_OPT_ASM)
#endif  // RUY_PLATFORM(X86)

}  // namespace ruy
