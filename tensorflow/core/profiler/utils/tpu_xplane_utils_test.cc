/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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
#include "tensorflow/core/profiler/utils/tpu_xplane_utils.h"

#include <vector>

#include "tensorflow/core/platform/test.h"
#include "tensorflow/core/profiler/protobuf/xplane.pb.h"
#include "tensorflow/core/profiler/utils/xplane_schema.h"
#include "tensorflow/core/profiler/utils/xplane_utils.h"

namespace tensorflow {
namespace profiler {
namespace {

using ::testing::UnorderedElementsAre;

TEST(TpuXPlaneUtilsTest, GetTensorCoreXPlanesFromXSpace) {
  XSpace xspace;

  XPlane* p1 = FindOrAddMutablePlaneWithName(&xspace, TpuPlaneName(0));
  XPlane* p2 = FindOrAddMutablePlaneWithName(&xspace, TpuPlaneName(1));
  FindOrAddMutablePlaneWithName(&xspace, TpuPlaneName(2) + "Postfix");

  std::vector<const XPlane*> xplanes = FindTensorCorePlanes(xspace);

  EXPECT_THAT(xplanes, UnorderedElementsAre(p1, p2));
}

TEST(TpuXPlaneUtilsTest, GetMutableTensorCoreXPlanesFromXSpace) {
  XSpace xspace;
  XPlane* p1 = FindOrAddMutablePlaneWithName(&xspace, TpuPlaneName(0));
  XPlane* p2 = FindOrAddMutablePlaneWithName(&xspace, TpuPlaneName(1));

  FindOrAddMutablePlaneWithName(&xspace, TpuPlaneName(2) + "Postfix");

  std::vector<XPlane*> xplanes = FindMutableTensorCorePlanes(&xspace);

  EXPECT_THAT(xplanes, UnorderedElementsAre(p1, p2));
}

}  // namespace
}  // namespace profiler
}  // namespace tensorflow
