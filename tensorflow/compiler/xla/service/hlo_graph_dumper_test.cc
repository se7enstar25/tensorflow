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

#include "tensorflow/compiler/xla/service/hlo_graph_dumper.h"

#include "absl/strings/str_cat.h"
#include "tensorflow/compiler/xla/literal_util.h"
#include "tensorflow/compiler/xla/service/hlo_computation.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_opcode.h"
#include "tensorflow/compiler/xla/test.h"
#include "tensorflow/compiler/xla/tests/hlo_test_base.h"
#include "tensorflow/compiler/xla/tests/test_utils.h"
#include "tensorflow/compiler/xla/xla.pb.h"

namespace xla {
namespace {

using absl::StrCat;
using ::testing::HasSubstr;

using HloGraphDumperTest = HloTestBase;

std::string TestName() {
  return ::testing::UnitTest::GetInstance()->current_test_info()->name();
}

TEST_F(HloGraphDumperTest, NestedFusion) {
  HloComputation::Builder b("b");

  // Build param0 + param1 + param2 + param3 + param4.
  auto shape = ShapeUtil::MakeShape(F32, {10, 100});
  std::vector<HloInstruction*> params;
  for (int i = 0; i <= 4; ++i) {
    params.push_back(b.AddInstruction(
        HloInstruction::CreateParameter(i, shape, StrCat("param", i))));
  }
  std::vector<HloInstruction*> sums;
  sums.push_back(b.AddInstruction(HloInstruction::CreateBinary(
      shape, HloOpcode::kAdd, params[0], params[1])));
  for (int i = 0; i <= 2; ++i) {
    sums.push_back(b.AddInstruction(HloInstruction::CreateBinary(
        shape, HloOpcode::kAdd, sums[i], params[i + 2])));
  }
  HloModuleConfig config;
  HloModule m(TestName(), config);
  m.AddEntryComputation(b.Build());
  HloComputation* root_computation = m.entry_computation();

  // Fuse into fusion(param0 + param1 + param2 + param3 + param4).
  auto* outer_fusion = root_computation->CreateFusionInstruction(
      {sums[3], sums[2], sums[1], sums[0]}, HloInstruction::FusionKind::kLoop);

  // Fusing invalidates the pointers in sums -- the instructions are cloned when
  // they're moved to the new computation.  Get the updated pointers to sums.
  std::vector<HloInstruction*> fused_sums;
  for (auto* instr : outer_fusion->fused_instructions_computation()
                         ->MakeInstructionPostOrder()) {
    if (instr->opcode() == HloOpcode::kAdd) {
      fused_sums.push_back(instr);
    }
  }

  // Fuse into fusion(fusion(param0 + param1 + param2) + param3 + param4).
  auto* inner_fusion =
      outer_fusion->fused_instructions_computation()->CreateFusionInstruction(
          {fused_sums[1], fused_sums[0]}, HloInstruction::FusionKind::kLoop);

  // Generate the graph; all nodes should be present.
  TF_ASSERT_OK_AND_ASSIGN(
      std::string graph,
      RenderGraph(*root_computation, /*label=*/"", DebugOptions(),
                  RenderedGraphFormat::kDot));
  for (const HloComputation* computation :
       {root_computation,  //
        inner_fusion->fused_instructions_computation(),
        outer_fusion->fused_instructions_computation()}) {
    for (const HloInstruction* instruction : computation->instructions()) {
      EXPECT_THAT(graph, HasSubstr(instruction->name()));
    }
  }

  // Dump a neighborhood around one of the inner sum nodes.  We don't really
  // care that the outer nodes are omitted -- whether they are or not is based
  // fiddly heuristics -- but we do care that the node we asked for is printed.
  const HloInstruction* inner_sum = nullptr;
  for (const HloInstruction* instruction :
       inner_fusion->fused_instructions_computation()->instructions()) {
    if (instruction->opcode() == HloOpcode::kAdd) {
      inner_sum = instruction;
      break;
    }
  }
  ASSERT_NE(inner_sum, nullptr);
  TF_ASSERT_OK_AND_ASSIGN(std::string neighborhood_graph,
                          RenderNeighborhoodAround(*inner_sum, /*radius=*/1,
                                                   RenderedGraphFormat::kDot));
  EXPECT_THAT(neighborhood_graph, HasSubstr(inner_sum->name()));
}

TEST_F(HloGraphDumperTest, Constant) {
  HloComputation::Builder b("b");
  auto instruction = b.AddInstruction(
      HloInstruction::CreateConstant(LiteralUtil::CreateR0<float>(-42)));
  instruction->SetAndSanitizeName("i_am_a_constant_root_instruction");
  HloModuleConfig config;
  HloModule m(TestName(), config);
  HloComputation* root_computation = m.AddEntryComputation(b.Build());
  TF_ASSERT_OK_AND_ASSIGN(
      std::string graph,
      RenderGraph(*root_computation, /*label=*/"an_empty_graph", DebugOptions(),
                  RenderedGraphFormat::kDot));
  EXPECT_THAT(graph, HasSubstr("an_empty_graph"));
  EXPECT_THAT(graph, Not(HasSubstr("i_am_a_constant_root_instruction")));
}

TEST_F(HloGraphDumperTest, TupleConstant) {
  Shape tuple_shape = ShapeUtil::MakeTupleShape(
      {ShapeUtil::MakeShape(F32, {3, 2}), ShapeUtil::MakeShape(S32, {4, 5})});
  HloComputation::Builder b("b");
  auto constant = b.AddInstruction(
      HloInstruction::CreateConstant(Literal::CreateFromShape(tuple_shape)));
  auto gte = b.AddInstruction(HloInstruction::CreateGetTupleElement(
      ShapeUtil::MakeShape(F32, {3, 2}), constant, 0));

  HloModuleConfig config;
  HloModule m(TestName(), config);
  HloComputation* root_computation = m.AddEntryComputation(b.Build(gte));
  TF_ASSERT_OK_AND_ASSIGN(
      std::string graph,
      RenderGraph(*root_computation, /*label=*/"tuple_constant", DebugOptions(),
                  RenderedGraphFormat::kDot));
  EXPECT_THAT(graph, HasSubstr("tuple_constant"));
  EXPECT_THAT(graph, HasSubstr("constant (f32[3,2], s32[4,5])"));
}

TEST_F(HloGraphDumperTest, Compare) {
  const char* hlo_string = R"(
    HloModule comp

    ENTRY comp {
      param.0 = f32[10] parameter(0)
      param.1 = f32[10] parameter(1)
      ROOT lt = pred[10] compare(param.0, param.1), direction=LT
    })";
  TF_ASSERT_OK_AND_ASSIGN(auto module,
                          ParseAndReturnVerifiedModule(hlo_string));
  TF_ASSERT_OK_AND_ASSIGN(
      std::string graph,
      RenderGraph(*module->entry_computation(), /*label=*/"tuple_constant",
                  DebugOptions(), RenderedGraphFormat::kDot));
  EXPECT_THAT(graph, HasSubstr("direction=LT"));
}

}  // anonymous namespace
}  // namespace xla
