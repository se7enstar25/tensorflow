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

#include "tensorflow/core/kernels/data/range_dataset_op.h"

#include "tensorflow/core/kernels/data/dataset_test_base.h"

namespace tensorflow {
namespace data {
namespace {

constexpr char kNodeName[] = "range_dataset";
constexpr char kIteratorPrefix[] = "Iterator";

class RangeDatasetOpTest : public DatasetOpsTestBase {
 protected:
  // Creates a new RangeDataset op kernel context.
  Status CreateRangeDatasetContext(
      OpKernel* const range_kernel,
      gtl::InlinedVector<TensorValue, 4>* const inputs,
      std::unique_ptr<OpKernelContext>* range_context) {
    TF_RETURN_IF_ERROR(CheckOpKernelInput(*range_kernel, *inputs));
    TF_RETURN_IF_ERROR(
        CreateOpKernelContext(range_kernel, inputs, range_context));
    return Status::OK();
  }
};

struct TestCase {
  int64 start;
  int64 stop;
  int64 step;
  std::vector<Tensor> expected_outputs;
  DataTypeVector expected_output_dtypes;
  std::vector<PartialTensorShape> expected_output_shapes;
  int64 expected_cardinality;
  std::vector<int> breakpoints;
};

TestCase PositiveStepTestCase() {
  return {/*start*/ 0,
          /*stop*/ 10,
          /*step*/ 3,
          /*expected_outputs*/
          {DatasetOpsTestBase::CreateTensor<int64>(TensorShape({}), {0}),
           DatasetOpsTestBase::CreateTensor<int64>(TensorShape({}), {3}),
           DatasetOpsTestBase::CreateTensor<int64>(TensorShape({}), {6}),
           DatasetOpsTestBase::CreateTensor<int64>(TensorShape({}), {9})},
          /*expected_output_dtypes*/ {DT_INT64},
          /*expected_output_shapes*/ {PartialTensorShape({})},
          /*expected_cardinality*/ 4,
          /*breakpoints*/ {0, 1, 4}};
}

TestCase NegativeStepTestCase() {
  return {/*start*/ 10,
          /*stop*/ 0,
          /*step*/ -3,
          /*expected_outputs*/
          {DatasetOpsTestBase::CreateTensor<int64>(TensorShape({}), {10}),
           DatasetOpsTestBase::CreateTensor<int64>(TensorShape({}), {7}),
           DatasetOpsTestBase::CreateTensor<int64>(TensorShape({}), {4}),
           DatasetOpsTestBase::CreateTensor<int64>(TensorShape({}), {1})},
          /*expected_output_dtypes*/ {DT_INT64},
          /*expected_output_shapes*/ {PartialTensorShape({})},
          /*expected_cardinality*/ 4,
          /*breakpoints*/ {0, 1, 4}};
}

TestCase ZeroStepTestCase() {
  return {/*start*/ 0,
          /*stop*/ 10,
          /*step*/ 0,
          /*expected_outputs*/ {},
          /*expected_output_dtypes*/ {},
          /*expected_output_shapes*/ {},
          /*expected_cardinality*/ 0,
          /*breakpoints*/ {}};
}

class ParameterizedRangeDatasetOpTest
    : public RangeDatasetOpTest,
      public ::testing::WithParamInterface<TestCase> {};

TEST_P(ParameterizedRangeDatasetOpTest, APIs) {
  int thread_num = 2, cpu_num = 2;
  TF_ASSERT_OK(InitThreadPool(thread_num));
  TF_ASSERT_OK(InitFunctionLibraryRuntime({}, cpu_num));

  TestCase test_case = GetParam();
  Tensor start = CreateTensor<int64>(TensorShape({}), {test_case.start});
  Tensor stop = CreateTensor<int64>(TensorShape({}), {test_case.stop});
  Tensor step = CreateTensor<int64>(TensorShape({}), {test_case.step});
  gtl::InlinedVector<TensorValue, 4> inputs(
      {TensorValue(&start), TensorValue(&stop), TensorValue(&step)});

  std::unique_ptr<OpKernel> range_dataset_kernel;
  TF_ASSERT_OK(
      CreateRangeDatasetOpKernel<int64>(kNodeName, &range_dataset_kernel));
  std::unique_ptr<OpKernelContext> range_dataset_context;
  TF_ASSERT_OK(CreateRangeDatasetContext(range_dataset_kernel.get(), &inputs,
                                         &range_dataset_context));
  DatasetBase* range_dataset;
  TF_ASSERT_OK(CreateDataset(range_dataset_kernel.get(),
                             range_dataset_context.get(), &range_dataset));
  core::ScopedUnref scoped_unref(range_dataset);

  EXPECT_OK(EvaluateDatasetNodeName(*range_dataset, kNodeName));
  EXPECT_OK(EvaluateDatasetTypeString(
      *range_dataset, name_utils::OpName(RangeDatasetOp::kDatasetType)));
  EXPECT_OK(EvaluateDatasetOutputDtypes(*range_dataset,
                                        test_case.expected_output_dtypes));
  EXPECT_OK(EvaluateDatasetOutputShapes(*range_dataset,
                                        test_case.expected_output_shapes));
  EXPECT_OK(EvaluateDatasetCardinality(*range_dataset,
                                       test_case.expected_cardinality));
  EXPECT_OK(EvaluateDatasetSave(*range_dataset));

  std::unique_ptr<IteratorContext> iterator_context;
  TF_ASSERT_OK(
      CreateIteratorContext(range_dataset_context.get(), &iterator_context));
  std::unique_ptr<IteratorBase> iterator;
  TF_ASSERT_OK(range_dataset->MakeIterator(iterator_context.get(),
                                           kIteratorPrefix, &iterator));

  EXPECT_OK(EvaluateIteratorOutputDtypes(*iterator,
                                         test_case.expected_output_dtypes));
  EXPECT_OK(EvaluateIteratorOutputShapes(*iterator,
                                         test_case.expected_output_shapes));
  EXPECT_OK(EvaluateIteratorPrefix(
      *iterator, name_utils::IteratorPrefix(RangeDatasetOp::kDatasetType,
                                            kIteratorPrefix)));
  EXPECT_OK(EvaluateIteratorGetNext(iterator.get(), iterator_context.get(),
                                    test_case.expected_outputs, true));

  EXPECT_OK(EvaluateIteratorSerialization(
      *range_dataset, iterator_context.get(), kIteratorPrefix,
      test_case.expected_outputs, test_case.breakpoints));
}

TEST_F(RangeDatasetOpTest, ZeroStep) {
  int thread_num = 2, cpu_num = 2;
  TF_ASSERT_OK(InitThreadPool(thread_num));
  TF_ASSERT_OK(InitFunctionLibraryRuntime({}, cpu_num));

  TestCase test_case = ZeroStepTestCase();
  Tensor start = CreateTensor<int64>(TensorShape({}), {test_case.start});
  Tensor stop = CreateTensor<int64>(TensorShape({}), {test_case.stop});
  Tensor step = CreateTensor<int64>(TensorShape({}), {test_case.step});
  gtl::InlinedVector<TensorValue, 4> inputs(
      {TensorValue(&start), TensorValue(&stop), TensorValue(&step)});

  std::unique_ptr<OpKernel> range_dataset_kernel;
  TF_ASSERT_OK(
      CreateRangeDatasetOpKernel<int64>(kNodeName, &range_dataset_kernel));
  std::unique_ptr<OpKernelContext> range_dataset_context;
  TF_ASSERT_OK(CreateRangeDatasetContext(range_dataset_kernel.get(), &inputs,
                                         &range_dataset_context));
  DatasetBase* range_dataset;
  EXPECT_EQ(CreateDataset(range_dataset_kernel.get(),
                          range_dataset_context.get(), &range_dataset)
                .code(),
            tensorflow::error::INVALID_ARGUMENT);
}

INSTANTIATE_TEST_SUITE_P(
    RangeDatasetOpTest, ParameterizedRangeDatasetOpTest,
    ::testing::ValuesIn(std::vector<TestCase>({PositiveStepTestCase(),
                                               NegativeStepTestCase()})));

}  // namespace
}  // namespace data
}  // namespace tensorflow
