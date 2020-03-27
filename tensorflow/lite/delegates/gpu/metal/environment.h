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

#ifndef TENSORFLOW_LITE_DELEGATES_GPU_METAL_ENVIRONMENT_H_
#define TENSORFLOW_LITE_DELEGATES_GPU_METAL_ENVIRONMENT_H_

#include <string>

namespace tflite {
namespace gpu {
namespace metal {

enum class GpuType {
  kUnknown,
  kA7,   // iPhone 5s, iPad Air, iPad Mini 2, iPad Mini 3.
  kA8,   // A8 iPhone 6, A8X iPad Air 2, iPad Mini 4.
  kA9,   // A9 iPhone 6s, iPad (2017), A9X iPad Pro (1st generation).
  kA10,  // iPhone 7, iPad (2018), A10X iPad Pro (2nd generation).
  kA11,  // iPhone 8/X.
  kA12,  // iPhone Xs.
};

enum class AppleGPU {
  kUnknown,
  kA7,
  kA8,
  kA8X,
  kA9,
  kA9X,
  kA10,
  kA10X,
  kA11,
  kA12,
  kA12X,
  kA12Z,
  kA13,
};

struct AppleGPUInfo {
  AppleGPUInfo() = default;
  explicit AppleGPUInfo(const std::string& device_name);
  AppleGPU gpu_type;

  bool IsBionic() const;

  // floating point rounding mode
  bool IsRoundToNearestSupported() const;

  int GetComputeUnitsCount() const;
};

struct DeviceInfo {
  DeviceInfo() = default;
  explicit DeviceInfo(const std::string& device_name);
  AppleGPUInfo apple_info;

  // floating point rounding mode
  bool IsRoundToNearestSupported() const;

  int GetComputeUnitsCount() const;
};

GpuType GetGpuType();

}  // namespace metal
}  // namespace gpu
}  // namespace tflite

#endif  // TENSORFLOW_LITE_DELEGATES_GPU_METAL_ENVIRONMENT_H_
