# Copyright 2018 The TensorFlow Authors. All Rights Reserved.
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
"""Tests for common utilities in this package."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.contrib.quantize.python import common
from tensorflow.python.client import session
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import ops
from tensorflow.python.framework import test_util
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import variable_scope
from tensorflow.python.ops import variables
from tensorflow.python.platform import googletest


class CommonTest(test_util.TensorFlowTestCase):

  def testCreateOrGetQuantizationStep(self):
    self._TestCreateOrGetQuantizationStep(False)

  def testCreateOrGetQuantizationStepResourceVar(self):
    self._TestCreateOrGetQuantizationStep(True)

  def _TestCreateOrGetQuantizationStep(self, use_resource):
    g = ops.Graph()
    with session.Session(graph=g) as sess:
      variable_scope.get_variable_scope().set_use_resource(use_resource)
      quantization_step_tensor = common.CreateOrGetQuantizationStep()

      # Check that operations are added to the graph.
      num_nodes = len(g.get_operations())
      self.assertGreater(num_nodes, 0)

      # Check that getting the quantization step doesn't change the graph.
      get_quantization_step_tensor = common.CreateOrGetQuantizationStep()
      self.assertEqual(quantization_step_tensor, get_quantization_step_tensor)
      self.assertEqual(num_nodes, len(g.get_operations()))

      # Ensure that running the graph increments the quantization step.
      sess.run(variables.global_variables_initializer())
      step_val = sess.run(quantization_step_tensor)
      self.assertEqual(step_val, 1)

      # Ensure that even running a graph that depends on the quantization step
      # multiple times only executes it once.
      a = quantization_step_tensor + 1
      b = a + quantization_step_tensor
      _, step_val = sess.run([b, quantization_step_tensor])
      self.assertEqual(step_val, 2)

  def testRerouteTensor(self):
    a = constant_op.constant(1, name='a')
    b = constant_op.constant(2, name='b')
    c = constant_op.constant(3, name='c')
    d = constant_op.constant(4, name='d')

    add_ac = math_ops.add(a, c)
    add_ad = math_ops.add(a, d)

    # Ensure that before rerouting the inputs are what we think.
    self._CheckOpHasInputs(add_ac.op, [a, c])
    self._CheckOpHasInputs(add_ad.op, [a, d])

    # references to tensor a should be replaced with b for all ops in
    # can_modify. This means add_ac will be changed but add_ad will not.
    common.RerouteTensor(b, a, can_modify=[add_ac.op])
    self._CheckOpHasInputs(add_ac.op, [b, c])
    self._CheckOpHasInputs(add_ad.op, [a, d])

  def _CheckOpHasInputs(self, op, inputs):
    for i in inputs:
      self.assertIn(i, op.inputs)


if __name__ == '__main__':
  googletest.main()
