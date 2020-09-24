# Copyright 2020 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Tests for tf.data placement within tf.functions."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from absl.testing import parameterized

from tensorflow.python.data.kernel_tests import test_base
from tensorflow.python.data.ops import dataset_ops
from tensorflow.python.eager import def_function
from tensorflow.python.framework import combinations
from tensorflow.python.framework import config
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import ops
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.platform import test


@combinations.generate(test_base.v2_eager_only_combinations())
class PlacementTest(test_base.DatasetTestBase, parameterized.TestCase):
  """Tests for tf.data placement within tf.functions.

  Specifically, tf.data dataset tensors cannot be copied between devices. These
  tests verify the ops are placed in a way that avoids this.
  """

  def setUp(self):
    super(PlacementTest, self).setUp()
    # Grappler optimizations can affect whether the placement issues occur,
    # since they may inadvertently rewrite nodes and edges in a way that removes
    # cross-device copies.
    config.set_optimizer_experimental_options({"disable_meta_optimizer": True})

  def testWhileWithCapturedDataset(self):
    dataset = dataset_ops.Dataset.range(10)

    @def_function.function
    def f():
      total = constant_op.constant(0, dtypes.int64)
      for _ in math_ops.range(1):
        for elem in dataset:
          total += elem
      return total

    self.assertEqual(f().numpy(), 45)

  def testWhile(self):
    self.skipTest("b/166625126")

    @def_function.function
    def f():
      dataset = dataset_ops.Dataset.range(10)
      total = constant_op.constant(0, dtypes.int64)
      for _ in math_ops.range(1):
        for elem in dataset:
          total += elem
      return total

    self.assertEqual(f().numpy(), 45)

  def testCondWithPlacement(self):
    # When the cond op is explicitly placed, there shouldn't be cross-device
    # copies.
    @def_function.function
    def f():
      dataset = dataset_ops.Dataset.range(10)

      def fn():
        return dataset.map(lambda x: x+1)

      c = constant_op.constant(2)
      with ops.device("/cpu:0"):
        a = control_flow_ops.cond(math_ops.equal(c, 2), fn, fn)
        iterator = iter(a)
        nxt = next(iterator)
      return nxt

    self.assertEqual(f(), 1)

  def testCondWithColocation(self):
    self.skipTest("b/166625126")
    # When the cond op is colocated with the dataset, there shouldn't be
    # cross-device copies.
    @def_function.function
    def f():
      dataset = dataset_ops.Dataset.range(8)

      def fn():
        return dataset.map(lambda x: x+1)

      c = constant_op.constant(2)
      with ops.colocate_with(dataset._variant_tensor):  # pylint:disable=protected-access
        a = control_flow_ops.cond(math_ops.equal(c, 2), fn, fn)
        iterator = iter(a)
        nxt = next(iterator)
      return nxt

    self.assertEqual(f().numpy(), 1)

  def testCond(self):
    self.skipTest("b/166625126")
    # Ideally, placer should avoid cross-device copies even when the cond op
    # has no placement constraints.
    @def_function.function
    def f():
      dataset = dataset_ops.Dataset.range(8)

      def fn():
        return dataset.map(lambda x: x+1)

      c = constant_op.constant(2)
      a = control_flow_ops.cond(math_ops.equal(c, 2), fn, fn)
      iterator = iter(a)
      nxt = next(iterator)
      return nxt

    self.assertEqual(f().numpy(), 1)

  def testId(self):
    self.skipTest("b/166625126")
    # Ideally, placer should know that Identity(dataset) should be on the same
    # device as the dataset.
    @def_function.function
    def f():
      dataset = dataset_ops.Dataset.range(10)
      dataset = array_ops.identity(dataset)
      return dataset
    f()

if __name__ == "__main__":
  test.main()
