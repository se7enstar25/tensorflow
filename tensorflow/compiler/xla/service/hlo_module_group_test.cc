/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/compiler/xla/service/hlo_module_group.h"

#include "tensorflow/compiler/xla/service/hlo.pb.h"
#include "tensorflow/compiler/xla/service/hlo_matchers.h"
#include "tensorflow/compiler/xla/service/hlo_parser.h"
#include "tensorflow/compiler/xla/test.h"
#include "tensorflow/compiler/xla/tests/hlo_test_base.h"
#include "tensorflow/core/lib/core/status_test_util.h"

namespace xla {

namespace {

namespace op = ::xla::testing::opcode_matchers;

class HloModuleGroupTest : public HloTestBase {
 protected:
  HloModuleGroupTest() = default;
};

TEST_F(HloModuleGroupTest, SingleModule) {
  const string text = R"(
HloModule simple_module

ENTRY %entry (x: f32[], y: f32[]) -> f32[] {
  %x = f32[] parameter(0)
  %y = f32[] parameter(1)
  ROOT %add = f32[] add(%x, %y)
}
)";
  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module,
                          ParseHloString(text));
  HloModuleGroup group(TestName(), std::move(module));

  EXPECT_EQ(group.modules().size(), 1);
  EXPECT_THAT(
      group.module(0).entry_computation()->instructions(),
      ::testing::ElementsAre(op::Parameter(), op::Parameter(), op::Add()));

  TF_ASSERT_OK_AND_ASSIGN(HloModuleGroup group_copy,
                          HloModuleGroup::CreateFromProto(
                              group.ToProto(), {group.module(0).config()}));
  EXPECT_EQ(group_copy.modules().size(), 1);
  EXPECT_THAT(
      group_copy.module(0).entry_computation()->instructions(),
      ::testing::ElementsAre(op::Parameter(), op::Parameter(), op::Add()));

  std::vector<std::unique_ptr<HloModule>> modules = group.ConsumeModules();
  EXPECT_EQ(modules.size(), 1);
  EXPECT_EQ(group.modules().size(), 0);
}

TEST_F(HloModuleGroupTest, MultipleModules) {
  const string text_0 = R"(
HloModule module0

ENTRY %entry (x: f32[], y: f32[]) -> f32[] {
  %x = f32[] parameter(0)
  %y = f32[] parameter(1)
  ROOT %add = f32[] add(%x, %y)
}
)";
  const string text_1 = R"(
HloModule module1

ENTRY %entry (a: f32[]) -> f32[] {
  ROOT %a = f32[] parameter(0)
}
)";
  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module_0,
                          ParseHloString(text_0));
  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module_1,
                          ParseHloString(text_1));
  std::vector<std::unique_ptr<HloModule>> modules;
  modules.push_back(std::move(module_0));
  modules.push_back(std::move(module_1));
  HloModuleGroup group(TestName(), absl::MakeSpan(modules));
  EXPECT_EQ(group.modules().size(), 2);
  EXPECT_THAT(
      group.module(0).entry_computation()->instructions(),
      ::testing::ElementsAre(op::Parameter(), op::Parameter(), op::Add()));
  EXPECT_THAT(group.module(1).entry_computation()->instructions(),
              ::testing::ElementsAre(op::Parameter()));

  TF_ASSERT_OK_AND_ASSIGN(HloModuleGroup group_copy,
                          HloModuleGroup::CreateFromProto(
                              group.ToProto(), {group.module(0).config(),
                                                group.module(1).config()}));
  EXPECT_EQ(group_copy.modules().size(), 2);
}

TEST_F(HloModuleGroupTest, BuildModuleGroupByPushBack) {
  const string text_0 = R"(
HloModule module0

ENTRY %entry (x: f32[], y: f32[]) -> f32[] {
  %x = f32[] parameter(0)
  %y = f32[] parameter(1)
  ROOT %add = f32[] add(%x, %y)
}
)";
  const string text_1 = R"(
HloModule module1

ENTRY %entry (a: f32[]) -> f32[] {
  ROOT %a = f32[] parameter(0)
}
)";
  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module_0,
                          ParseHloString(text_0));
  TF_ASSERT_OK_AND_ASSIGN(std::unique_ptr<HloModule> module_1,
                          ParseHloString(text_1));
  HloModuleGroup group(TestName());
  group.push_back(std::move(module_0));
  group.push_back(std::move(module_1));

  EXPECT_EQ(group.modules().size(), 2);
  EXPECT_THAT(
      group.module(0).entry_computation()->instructions(),
      ::testing::ElementsAre(op::Parameter(), op::Parameter(), op::Add()));
  EXPECT_THAT(group.module(1).entry_computation()->instructions(),
              ::testing::ElementsAre(op::Parameter()));
}

}  // namespace

}  // namespace xla
