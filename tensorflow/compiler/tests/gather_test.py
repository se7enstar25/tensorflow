# Copyright 2017 The TensorFlow Authors. All Rights Reserved.
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
"""Functional tests for XLA Gather Op."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np

from tensorflow.compiler.tests import xla_test
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import dtypes
from tensorflow.python.ops import array_ops
from tensorflow.python.platform import test

_TEST_TYPES = [dtypes.float32]


class GatherTest(xla_test.XLATestCase):

  def _buildParams(self, data, dtype):
    data = data.astype(dtype.as_numpy_dtype)
    # For complex types, adds an index-dependent imaginary component so we can
    # tell we got the right value.
    if dtype.is_complex:
      return data + 10j * data
    return data

  def testScalar1D(self):
    with self.test_session() as session, self.test_scope():
      data = np.array([0, 1, 2, 3, 7, 5])
      for dtype in _TEST_TYPES:
        for indices in 4, [1, 2, 2, 4, 5]:
          params_np = self._buildParams(data, dtype)
          params = array_ops.placeholder(dtype=dtype)
          indices_tf = constant_op.constant(indices)
          gather_t = array_ops.gather(params, indices_tf)
          gather_val = session.run(gather_t, feed_dict={params: params_np})
          np_val = params_np[indices]
          self.assertAllEqual(np_val, gather_val)

  def testScalar2D(self):
    with self.test_session() as session, self.test_scope():
      data = np.array([[0, 1, 2], [3, 4, 5], [6, 7, 8], [9, 10, 11],
                       [12, 13, 14]])
      for dtype in _TEST_TYPES:
        for axis in 0, 1, -1:
          params_np = self._buildParams(data, dtype)
          params = array_ops.placeholder(dtype=dtype)
          indices = constant_op.constant(2)
          gather_t = array_ops.gather(params, indices, axis=axis)
          gather_val = session.run(gather_t, feed_dict={params: params_np})
          expected = np.take(params_np, 2, axis=axis)
          self.assertAllEqual(expected, gather_val)

  def testSimpleTwoD32(self):
    with self.test_session() as session, self.test_scope():
      data = np.array([[0, 1, 2], [3, 4, 5], [6, 7, 8], [9, 10, 11],
                       [12, 13, 14]])
      for dtype in _TEST_TYPES:
        for axis in 0, 1, -1:
          params_np = self._buildParams(data, dtype)
          params = array_ops.placeholder(dtype=dtype)
          # The indices must be in bounds for any axis.
          indices = constant_op.constant([0, 1, 0, 2])
          gather_t = array_ops.gather(params, indices, axis=axis)
          gather_val = session.run(gather_t, feed_dict={params: params_np})
          expected = np.take(params_np, [0, 1, 0, 2], axis=axis)
          self.assertAllEqual(expected, gather_val)

  def testHigherRank(self):
    # Check that scalar and empty indices shapes work as well.
    shape = (2, 1, 3, 2)
    for indices_shape in (), (0,), (2, 0), (2, 3):
      for dtype in _TEST_TYPES:
        for axis in 0, 1, 2, 3, -1, -2:
          params = self._buildParams(np.random.randn(*shape), dtype)
          indices = np.random.randint(shape[axis], size=indices_shape)
          with self.test_session() as sess, self.test_scope():
            tf_params = array_ops.placeholder(dtype=dtype)
            tf_indices = constant_op.constant(indices, dtype=dtypes.int32)
            gather = array_ops.gather(tf_params, tf_indices, axis=axis)
            gather_value = sess.run(gather, feed_dict={tf_params: params})
            gather_np = np.take(params, indices, axis=axis)
            self.assertAllEqual(gather_np, gather_value)


if __name__ == "__main__":
  test.main()
