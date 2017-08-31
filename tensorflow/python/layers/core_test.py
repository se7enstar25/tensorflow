# Copyright 2015 The TensorFlow Authors. All Rights Reserved.
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
"""Tests for tf.layers.core."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np

from tensorflow.python.eager import context
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import ops
from tensorflow.python.framework import tensor_shape
from tensorflow.python.framework import test_util
from tensorflow.python.layers import core as core_layers
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import init_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import nn_ops
from tensorflow.python.ops import random_ops
from tensorflow.python.ops import variable_scope
from tensorflow.python.ops import variables
from tensorflow.python.platform import test


class DenseTest(test.TestCase):

  @test_util.run_in_graph_and_eager_modes()
  def testDenseProperties(self):
    dense = core_layers.Dense(2, activation=nn_ops.relu, name='my_dense')
    self.assertEqual(dense.units, 2)
    self.assertEqual(dense.activation, nn_ops.relu)
    self.assertEqual(dense.kernel_regularizer, None)
    self.assertEqual(dense.bias_regularizer, None)
    self.assertEqual(dense.activity_regularizer, None)
    self.assertEqual(dense.use_bias, True)

    # Test auto-naming
    dense = core_layers.Dense(2, activation=nn_ops.relu)
    dense.apply(random_ops.random_uniform((5, 2)))
    self.assertEqual(dense.name, 'dense_1')
    dense = core_layers.Dense(2, activation=nn_ops.relu)
    dense.apply(random_ops.random_uniform((5, 2)))
    self.assertEqual(dense.name, 'dense_2')

  @test_util.run_in_graph_and_eager_modes()
  def testCall(self):
    dense = core_layers.Dense(2, activation=nn_ops.relu, name='my_dense')
    inputs = random_ops.random_uniform((5, 4), seed=1)
    outputs = dense(inputs)
    self.assertListEqual([5, 2], outputs.get_shape().as_list())
    self.assertListEqual(dense.variables, [dense.kernel, dense.bias])
    self.assertListEqual(dense.trainable_variables, [dense.kernel, dense.bias])
    self.assertListEqual(dense.non_trainable_variables, [])
    self.assertEqual(
        len(ops.get_collection(ops.GraphKeys.TRAINABLE_VARIABLES)), 2)
    self.assertEqual(dense.kernel.name, 'my_dense/kernel:0')
    self.assertEqual(dense.bias.name, 'my_dense/bias:0')

  @test_util.run_in_graph_and_eager_modes()
  def testCallTensorDot(self):
    dense = core_layers.Dense(2, activation=nn_ops.relu, name='my_dense')
    inputs = random_ops.random_uniform((5, 4, 3), seed=1)
    outputs = dense(inputs)
    self.assertListEqual([5, 4, 2], outputs.get_shape().as_list())

  @test_util.run_in_graph_and_eager_modes()
  def testNoBias(self):
    dense = core_layers.Dense(2, use_bias=False, name='my_dense')
    inputs = random_ops.random_uniform((5, 2), seed=1)
    _ = dense(inputs)
    self.assertListEqual(dense.variables, [dense.kernel])
    self.assertListEqual(dense.trainable_variables, [dense.kernel])
    self.assertListEqual(dense.non_trainable_variables, [])
    self.assertEqual(
        len(ops.get_collection(ops.GraphKeys.TRAINABLE_VARIABLES)), 1)
    self.assertEqual(dense.kernel.name, 'my_dense/kernel:0')
    self.assertEqual(dense.bias, None)

  @test_util.run_in_graph_and_eager_modes()
  def testNonTrainable(self):
    dense = core_layers.Dense(2, trainable=False, name='my_dense')
    inputs = random_ops.random_uniform((5, 2), seed=1)
    _ = dense(inputs)
    self.assertListEqual(dense.variables, [dense.kernel, dense.bias])
    self.assertListEqual(dense.non_trainable_variables,
                         [dense.kernel, dense.bias])
    self.assertListEqual(dense.trainable_variables, [])
    self.assertEqual(
        len(ops.get_collection(ops.GraphKeys.TRAINABLE_VARIABLES)), 0)

  @test_util.run_in_graph_and_eager_modes()
  def testOutputShape(self):
    dense = core_layers.Dense(7, activation=nn_ops.relu, name='my_dense')
    inputs = random_ops.random_uniform((5, 3), seed=1)
    outputs = dense.apply(inputs)
    self.assertEqual(outputs.get_shape().as_list(), [5, 7])

    inputs = random_ops.random_uniform((5, 2, 3), seed=1)
    outputs = dense(inputs)
    self.assertEqual(outputs.get_shape().as_list(), [5, 2, 7])

    inputs = random_ops.random_uniform((1, 2, 4, 3), seed=1)
    outputs = dense.apply(inputs)
    self.assertEqual(outputs.get_shape().as_list(), [1, 2, 4, 7])

  def testCallOnPlaceHolder(self):
    inputs = array_ops.placeholder(dtype=dtypes.float32)
    dense = core_layers.Dense(4, name='my_dense')
    with self.assertRaises(ValueError):
      dense(inputs)

    inputs = array_ops.placeholder(dtype=dtypes.float32, shape=[None, None])
    dense = core_layers.Dense(4, name='my_dense')
    with self.assertRaises(ValueError):
      dense(inputs)

    inputs = array_ops.placeholder(
        dtype=dtypes.float32, shape=[None, None, None])
    dense = core_layers.Dense(4, name='my_dense')
    with self.assertRaises(ValueError):
      dense(inputs)

    inputs = array_ops.placeholder(dtype=dtypes.float32, shape=[None, 3])
    dense = core_layers.Dense(4, name='my_dense')
    dense(inputs)

    inputs = array_ops.placeholder(dtype=dtypes.float32, shape=[None, None, 3])
    dense = core_layers.Dense(4, name='my_dense')
    dense(inputs)

  @test_util.run_in_graph_and_eager_modes()
  def testActivation(self):
    dense = core_layers.Dense(2, activation=nn_ops.relu, name='dense1')
    inputs = random_ops.random_uniform((5, 3), seed=1)
    outputs = dense(inputs)
    if context.in_graph_mode():
      self.assertEqual(outputs.op.name, 'dense1/Relu')

    dense = core_layers.Dense(2, name='dense2')
    inputs = random_ops.random_uniform((5, 3), seed=1)
    outputs = dense(inputs)
    if context.in_graph_mode():
      self.assertEqual(outputs.op.name, 'dense2/BiasAdd')

  def testActivityRegularizer(self):
    regularizer = lambda x: math_ops.reduce_sum(x) * 1e-3
    dense = core_layers.Dense(
        2, name='my_dense', activity_regularizer=regularizer)
    inputs = random_ops.random_uniform((5, 3), seed=1)
    _ = dense(inputs)
    loss_keys = ops.get_collection(ops.GraphKeys.REGULARIZATION_LOSSES)
    self.assertEqual(len(loss_keys), 1)
    self.assertListEqual(dense.losses, loss_keys)

  def testKernelRegularizer(self):
    regularizer = lambda x: math_ops.reduce_sum(x) * 1e-3
    dense = core_layers.Dense(
        2, name='my_dense', kernel_regularizer=regularizer)
    inputs = random_ops.random_uniform((5, 3), seed=1)
    _ = dense(inputs)
    loss_keys = ops.get_collection(ops.GraphKeys.REGULARIZATION_LOSSES)
    self.assertEqual(len(loss_keys), 1)
    self.assertListEqual(dense.losses, loss_keys)

  def testKernelRegularizerWithReuse(self):
    regularizer = lambda x: math_ops.reduce_sum(x) * 1e-3
    inputs = random_ops.random_uniform((5, 3), seed=1)
    _ = core_layers.dense(
        inputs, 2, name='my_dense', kernel_regularizer=regularizer)
    self.assertEqual(
        len(ops.get_collection(ops.GraphKeys.REGULARIZATION_LOSSES)), 1)
    _ = core_layers.dense(
        inputs, 2, name='my_dense', kernel_regularizer=regularizer, reuse=True)
    self.assertEqual(
        len(ops.get_collection(ops.GraphKeys.REGULARIZATION_LOSSES)), 1)

  def testBiasRegularizer(self):
    regularizer = lambda x: math_ops.reduce_sum(x) * 1e-3
    dense = core_layers.Dense(2, name='my_dense', bias_regularizer=regularizer)
    inputs = random_ops.random_uniform((5, 3), seed=1)
    _ = dense(inputs)
    loss_keys = ops.get_collection(ops.GraphKeys.REGULARIZATION_LOSSES)
    self.assertEqual(len(loss_keys), 1)
    self.assertListEqual(dense.losses, loss_keys)

  @test_util.run_in_graph_and_eager_modes()
  def testFunctionalDense(self):
    inputs = random_ops.random_uniform((5, 3), seed=1)
    outputs = core_layers.dense(
        inputs, 2, activation=nn_ops.relu, name='my_dense')
    self.assertEqual(
        len(ops.get_collection(ops.GraphKeys.TRAINABLE_VARIABLES)), 2)
    if context.in_graph_mode():
      self.assertEqual(outputs.op.name, 'my_dense/Relu')
    self.assertEqual(outputs.get_shape().as_list(), [5, 2])

  @test_util.run_in_graph_and_eager_modes()
  def testFunctionalDenseTwice(self):
    inputs = random_ops.random_uniform((5, 3), seed=1)
    core_layers.dense(inputs, 2)
    vars1 = variables.trainable_variables()
    core_layers.dense(inputs, 2)
    vars2 = variables.trainable_variables()
    self.assertEqual(len(vars1), 2)
    self.assertEqual(len(vars2), 4)

  @test_util.run_in_graph_and_eager_modes()
  def testFunctionalDenseTwiceReuse(self):
    inputs = random_ops.random_uniform((5, 3), seed=1)
    core_layers.dense(inputs, 2, name='my_dense')
    vars1 = variables.trainable_variables()
    core_layers.dense(inputs, 2, name='my_dense', reuse=True)
    vars2 = variables.trainable_variables()
    self.assertEqual(vars1, vars2)

  @test_util.run_in_graph_and_eager_modes()
  def testFunctionalDenseTwiceReuseFromScope(self):
    with variable_scope.variable_scope('scope'):
      inputs = random_ops.random_uniform((5, 3), seed=1)
      core_layers.dense(inputs, 2, name='my_dense')
      vars1 = variables.trainable_variables()
    with variable_scope.variable_scope('scope', reuse=True):
      core_layers.dense(inputs, 2, name='my_dense')
      vars2 = variables.trainable_variables()
    self.assertEqual(vars1, vars2)

  @test_util.run_in_graph_and_eager_modes()
  def testFunctionalDenseInitializerFromScope(self):
    with variable_scope.variable_scope(
        'scope', initializer=init_ops.ones_initializer()):
      inputs = random_ops.random_uniform((5, 3), seed=1)
      core_layers.dense(inputs, 2)
      if context.in_graph_mode():
        self.evaluate(variables.global_variables_initializer())
      weights = variables.trainable_variables()
      self.assertEqual(len(weights), 2)
      # Check that the matrix weights got initialized to ones (from scope).
      self.assertAllClose(
          self.evaluate(weights[0].read_value()), np.ones((3, 2)))
      # Check that the bias still got initialized to zeros.
      self.assertAllClose(self.evaluate(weights[1].read_value()), np.zeros((2)))

  @test_util.run_in_graph_and_eager_modes()
  def testFunctionalDenseWithCustomGetter(self):
    called = [0]

    def custom_getter(getter, *args, **kwargs):
      called[0] += 1
      return getter(*args, **kwargs)

    with variable_scope.variable_scope('test', custom_getter=custom_getter):
      inputs = random_ops.random_uniform((5, 3), seed=1)
      core_layers.dense(inputs, 2)
    self.assertEqual(called[0], 2)

  @test_util.run_in_graph_and_eager_modes()
  def testFunctionalDenseInScope(self):
    with variable_scope.variable_scope('test'):
      inputs = random_ops.random_uniform((5, 3), seed=1)
      core_layers.dense(inputs, 2, name='my_dense')
      var = variables.trainable_variables()[0]
      self.assertEqual(var.name, 'test/my_dense/kernel:0')
    with variable_scope.variable_scope('test1') as scope:
      inputs = random_ops.random_uniform((5, 3), seed=1)
      core_layers.dense(inputs, 2, name=scope)
      var = variables.trainable_variables()[2]
      self.assertEqual(var.name, 'test1/kernel:0')
    with variable_scope.variable_scope('test2'):
      inputs = random_ops.random_uniform((5, 3), seed=1)
      core_layers.dense(inputs, 2)
      var = variables.trainable_variables()[4]
      self.assertEqual(var.name, 'test2/dense/kernel:0')

  @test_util.run_in_graph_and_eager_modes()
  def testComputeOutputShape(self):
    dense = core_layers.Dense(2, activation=nn_ops.relu, name='dense1')
    ts = tensor_shape.TensorShape
    # pylint: disable=protected-access
    with self.assertRaises(ValueError):
      dense._compute_output_shape(ts(None))
    with self.assertRaises(ValueError):
      dense._compute_output_shape(ts([]))
    with self.assertRaises(ValueError):
      dense._compute_output_shape(ts([1]))
    self.assertEqual(
        [None, 2],
        dense._compute_output_shape((None, 3)).as_list())
    self.assertEqual(
        [None, 2],
        dense._compute_output_shape(ts([None, 3])).as_list())
    self.assertEqual(
        [None, 4, 2],
        dense._compute_output_shape(ts([None, 4, 3])).as_list())
    # pylint: enable=protected-access

  @test_util.run_in_graph_and_eager_modes()
  def testConstraints(self):
    k_constraint = lambda x: x / math_ops.reduce_sum(x)
    b_constraint = lambda x: x / math_ops.reduce_max(x)
    dense = core_layers.Dense(2,
                              kernel_constraint=k_constraint,
                              bias_constraint=b_constraint)
    inputs = random_ops.random_uniform((5, 3), seed=1)
    dense(inputs)
    self.assertEqual(dense.kernel_constraint, k_constraint)
    self.assertEqual(dense.bias_constraint, b_constraint)


class DropoutTest(test.TestCase):

  @test_util.run_in_graph_and_eager_modes()
  def testDropoutProperties(self):
    dp = core_layers.Dropout(0.5, name='dropout')
    self.assertEqual(dp.rate, 0.5)
    self.assertEqual(dp.noise_shape, None)
    dp.apply(array_ops.ones(()))
    self.assertEqual(dp.name, 'dropout')

  @test_util.run_in_graph_and_eager_modes()
  def testBooleanLearningPhase(self):
    dp = core_layers.Dropout(0.5)
    inputs = array_ops.ones((5, 3))
    dropped = dp.apply(inputs, training=True)
    if context.in_graph_mode():
      self.evaluate(variables.global_variables_initializer())
    np_output = self.evaluate(dropped)
    self.assertAlmostEqual(0., np_output.min())
    dropped = dp.apply(inputs, training=False)
    np_output = self.evaluate(dropped)
    self.assertAllClose(np.ones((5, 3)), np_output)

  def testDynamicLearningPhase(self):
    with self.test_session() as sess:
      dp = core_layers.Dropout(0.5, seed=1)
      inputs = array_ops.ones((5, 5))
      training = array_ops.placeholder(dtype='bool')
      dropped = dp.apply(inputs, training=training)
      self.evaluate(variables.global_variables_initializer())
      np_output = sess.run(dropped, feed_dict={training: True})
      self.assertAlmostEqual(0., np_output.min())
      np_output = sess.run(dropped, feed_dict={training: False})
      self.assertAllClose(np.ones((5, 5)), np_output)

  @test_util.run_in_graph_and_eager_modes()
  def testCustomNoiseShape(self):
    inputs = array_ops.ones((5, 3, 2))
    noise_shape = [5, 1, 2]
    dp = core_layers.Dropout(0.5, noise_shape=noise_shape, seed=1)
    dropped = dp.apply(inputs, training=True)
    self.evaluate(variables.global_variables_initializer())
    np_output = self.evaluate(dropped)
    self.assertAlmostEqual(0., np_output.min())
    self.assertAllClose(np_output[:, 0, :], np_output[:, 1, :])

  @test_util.run_in_graph_and_eager_modes()
  def testFunctionalDropout(self):
    inputs = array_ops.ones((5, 5))
    dropped = core_layers.dropout(inputs, 0.5, training=True, seed=1)
    if context.in_graph_mode():
      self.evaluate(variables.global_variables_initializer())
    np_output = self.evaluate(dropped)
    self.assertAlmostEqual(0., np_output.min())
    dropped = core_layers.dropout(inputs, 0.5, training=False, seed=1)
    np_output = self.evaluate(dropped)
    self.assertAllClose(np.ones((5, 5)), np_output)

  def testDynamicRate(self):
    with self.test_session() as sess:
      rate = array_ops.placeholder(dtype='float32', name='rate')
      dp = core_layers.Dropout(rate, name='dropout')
      inputs = array_ops.ones((5, 5))
      dropped = dp.apply(inputs, training=True)
      sess.run(variables.global_variables_initializer())
      np_output = sess.run(dropped, feed_dict={rate: 0.5})
      self.assertAlmostEqual(0., np_output.min())
      np_output = sess.run(dropped, feed_dict={rate: 0.0})
      self.assertAllClose(np.ones((5, 5)), np_output)


if __name__ == '__main__':
  test.main()
