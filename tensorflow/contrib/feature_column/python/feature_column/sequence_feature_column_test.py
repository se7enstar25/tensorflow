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
"""Tests for sequential_feature_column."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np

from tensorflow.contrib.feature_column.python.feature_column import sequence_feature_column as sfc
from tensorflow.python.feature_column.feature_column import _LazyBuilder
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import errors
from tensorflow.python.framework import ops
from tensorflow.python.framework import sparse_tensor
from tensorflow.python.platform import test
from tensorflow.python.training import monitored_session


class SequenceInputLayerTest(test.TestCase):

  def test_embedding_column(self):
    vocabulary_size = 3
    sparse_input_a = sparse_tensor.SparseTensorValue(
        # example 0, ids [2]
        # example 1, ids [0, 1]
        indices=((0, 0), (1, 0), (1, 1)),
        values=(2, 0, 1),
        dense_shape=(2, 2))
    sparse_input_b = sparse_tensor.SparseTensorValue(
        # example 0, ids [1]
        # example 1, ids [2, 0]
        indices=((0, 0), (1, 0), (1, 1)),
        values=(1, 2, 0),
        dense_shape=(2, 2))

    embedding_dimension_a = 2
    embedding_values_a = (
        (1., 2.),  # id 0
        (3., 4.),  # id 1
        (5., 6.)  # id 2
    )
    embedding_dimension_b = 3
    embedding_values_b = (
        (11., 12., 13.),  # id 0
        (14., 15., 16.),  # id 1
        (17., 18., 19.)  # id 2
    )
    def _get_initializer(embedding_dimension, embedding_values):
      def _initializer(shape, dtype, partition_info):
        self.assertAllEqual((vocabulary_size, embedding_dimension), shape)
        self.assertEqual(dtypes.float32, dtype)
        self.assertIsNone(partition_info)
        return embedding_values
      return _initializer

    expected_input_layer = [
        # example 0, ids_a [2], ids_b [1]
        [[5., 6., 14., 15., 16.], [0., 0., 0., 0., 0.]],
        # example 1, ids_a [0, 1], ids_b [2, 0]
        [[1., 2., 17., 18., 19.], [3., 4., 11., 12., 13.]],
    ]
    expected_sequence_length = [1, 2]

    categorical_column_a = sfc.sequence_categorical_column_with_identity(
        key='aaa', num_buckets=vocabulary_size)
    embedding_column_a = sfc._sequence_embedding_column(
        categorical_column_a, dimension=embedding_dimension_a,
        initializer=_get_initializer(embedding_dimension_a, embedding_values_a))
    categorical_column_b = sfc.sequence_categorical_column_with_identity(
        key='bbb', num_buckets=vocabulary_size)
    embedding_column_b = sfc._sequence_embedding_column(
        categorical_column_b, dimension=embedding_dimension_b,
        initializer=_get_initializer(embedding_dimension_b, embedding_values_b))

    input_layer, sequence_length = sfc.sequence_input_layer(
        features={
            'aaa': sparse_input_a,
            'bbb': sparse_input_b,
        },
        # Test that columns are reordered alphabetically.
        feature_columns=[embedding_column_b, embedding_column_a])

    global_vars = ops.get_collection(ops.GraphKeys.GLOBAL_VARIABLES)
    self.assertItemsEqual(
        ('sequence_input_layer/aaa_embedding/embedding_weights:0',
         'sequence_input_layer/bbb_embedding/embedding_weights:0'),
        tuple([v.name for v in global_vars]))
    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(embedding_values_a, global_vars[0].eval(session=sess))
      self.assertAllEqual(embedding_values_b, global_vars[1].eval(session=sess))
      self.assertAllEqual(expected_input_layer, input_layer.eval(session=sess))
      self.assertAllEqual(
          expected_sequence_length, sequence_length.eval(session=sess))

  def test_indicator_column(self):
    vocabulary_size_a = 3
    sparse_input_a = sparse_tensor.SparseTensorValue(
        # example 0, ids [2]
        # example 1, ids [0, 1]
        indices=((0, 0), (1, 0), (1, 1)),
        values=(2, 0, 1),
        dense_shape=(2, 2))
    vocabulary_size_b = 2
    sparse_input_b = sparse_tensor.SparseTensorValue(
        # example 0, ids [1]
        # example 1, ids [1, 0]
        indices=((0, 0), (1, 0), (1, 1)),
        values=(1, 1, 0),
        dense_shape=(2, 2))

    expected_input_layer = [
        # example 0, ids_a [2], ids_b [1]
        [[0., 0., 1., 0., 1.], [0., 0., 0., 0., 0.]],
        # example 1, ids_a [0, 1], ids_b [1, 0]
        [[1., 0., 0., 0., 1.], [0., 1., 0., 1., 0.]],
    ]
    expected_sequence_length = [1, 2]

    categorical_column_a = sfc.sequence_categorical_column_with_identity(
        key='aaa', num_buckets=vocabulary_size_a)
    indicator_column_a = sfc._sequence_indicator_column(categorical_column_a)
    categorical_column_b = sfc.sequence_categorical_column_with_identity(
        key='bbb', num_buckets=vocabulary_size_b)
    indicator_column_b = sfc._sequence_indicator_column(categorical_column_b)
    input_layer, sequence_length = sfc.sequence_input_layer(
        features={
            'aaa': sparse_input_a,
            'bbb': sparse_input_b,
        },
        # Test that columns are reordered alphabetically.
        feature_columns=[indicator_column_b, indicator_column_a])

    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(expected_input_layer, input_layer.eval(session=sess))
      self.assertAllEqual(
          expected_sequence_length, sequence_length.eval(session=sess))

  def test_numeric_column(self):
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, values [[0.], [1]]
        # example 1, [[10.]]
        indices=((0, 0), (0, 1), (1, 0)),
        values=(0., 1., 10.),
        dense_shape=(2, 2))
    expected_input_layer = [
        [[0.], [1.]],
        [[10.], [0.]],
    ]
    expected_sequence_length = [2, 1]
    numeric_column = sfc.sequence_numeric_column('aaa')

    input_layer, sequence_length = sfc.sequence_input_layer(
        features={'aaa': sparse_input},
        feature_columns=[numeric_column])

    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(expected_input_layer, input_layer.eval(session=sess))
      self.assertAllEqual(
          expected_sequence_length, sequence_length.eval(session=sess))

  def test_numeric_column_multi_dim(self):
    """Tests sequence_input_layer for multi-dimensional numeric_column."""
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, values [[[0., 1.],  [2., 3.]], [[4., 5.],  [6., 7.]]]
        # example 1, [[[10., 11.],  [12., 13.]]]
        indices=((0, 0), (0, 1), (0, 2), (0, 3), (0, 4), (0, 5), (0, 6), (0, 7),
                 (1, 0), (1, 1), (1, 2), (1, 3)),
        values=(0., 1., 2., 3., 4., 5., 6., 7., 10., 11., 12., 13.),
        dense_shape=(2, 8))
    # The output of numeric_column._get_dense_tensor should be flattened.
    expected_input_layer = [
        [[0., 1., 2., 3.], [4., 5., 6., 7.]],
        [[10., 11., 12., 13.], [0., 0., 0., 0.]],
    ]
    expected_sequence_length = [2, 1]
    numeric_column = sfc.sequence_numeric_column('aaa', shape=(2, 2))

    input_layer, sequence_length = sfc.sequence_input_layer(
        features={'aaa': sparse_input},
        feature_columns=[numeric_column])

    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(expected_input_layer, input_layer.eval(session=sess))
      self.assertAllEqual(
          expected_sequence_length, sequence_length.eval(session=sess))


def _assert_sparse_tensor_value(test_case, expected, actual):
  test_case.assertEqual(np.int64, np.array(actual.indices).dtype)
  test_case.assertAllEqual(expected.indices, actual.indices)

  test_case.assertEqual(
      np.array(expected.values).dtype, np.array(actual.values).dtype)
  test_case.assertAllEqual(expected.values, actual.values)

  test_case.assertEqual(np.int64, np.array(actual.dense_shape).dtype)
  test_case.assertAllEqual(expected.dense_shape, actual.dense_shape)


class SequenceCategoricalColumnWithIdentityTest(test.TestCase):

  def test_get_sparse_tensors(self):
    column = sfc.sequence_categorical_column_with_identity(
        'aaa', num_buckets=3)
    inputs = sparse_tensor.SparseTensorValue(
        indices=((0, 0), (1, 0), (1, 1)),
        values=(1, 2, 0),
        dense_shape=(2, 2))
    expected_sparse_ids = sparse_tensor.SparseTensorValue(
        indices=((0, 0, 0), (1, 0, 0), (1, 1, 0)),
        values=np.array((1, 2, 0), dtype=np.int64),
        dense_shape=(2, 2, 1))

    id_weight_pair = column._get_sparse_tensors(_LazyBuilder({'aaa': inputs}))

    self.assertIsNone(id_weight_pair.weight_tensor)
    with monitored_session.MonitoredSession() as sess:
      _assert_sparse_tensor_value(
          self,
          expected_sparse_ids,
          id_weight_pair.id_tensor.eval(session=sess))

  def test_get_sparse_tensors_inputs3d(self):
    """Tests _get_sparse_tensors when the input is already 3D Tensor."""
    column = sfc.sequence_categorical_column_with_identity(
        'aaa', num_buckets=3)
    inputs = sparse_tensor.SparseTensorValue(
        indices=((0, 0, 0), (1, 0, 0), (1, 1, 0)),
        values=(1, 2, 0),
        dense_shape=(2, 2, 1))

    with self.assertRaisesRegexp(
        errors.InvalidArgumentError,
        r'Column aaa expected ID tensor of rank 2\.\s*'
        r'id_tensor shape:\s*\[2 2 1\]'):
      id_weight_pair = column._get_sparse_tensors(
          _LazyBuilder({'aaa': inputs}))
      with monitored_session.MonitoredSession() as sess:
        id_weight_pair.id_tensor.eval(session=sess)

  def test_sequence_length(self):
    column = sfc.sequence_categorical_column_with_identity(
        'aaa', num_buckets=3)
    inputs = sparse_tensor.SparseTensorValue(
        indices=((0, 0), (1, 0), (1, 1)),
        values=(1, 2, 0),
        dense_shape=(2, 2))
    expected_sequence_length = [1, 2]

    sequence_length = column._sequence_length(_LazyBuilder({'aaa': inputs}))

    with monitored_session.MonitoredSession() as sess:
      sequence_length = sess.run(sequence_length)
      self.assertAllEqual(expected_sequence_length, sequence_length)
      self.assertEqual(np.int64, sequence_length.dtype)

  def test_sequence_length_with_zeros(self):
    column = sfc.sequence_categorical_column_with_identity(
        'aaa', num_buckets=3)
    inputs = sparse_tensor.SparseTensorValue(
        indices=((1, 0), (3, 0), (3, 1)),
        values=(1, 2, 0),
        dense_shape=(5, 2))
    expected_sequence_length = [0, 1, 0, 2, 0]

    sequence_length = column._sequence_length(_LazyBuilder({'aaa': inputs}))

    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(
          expected_sequence_length, sequence_length.eval(session=sess))


class SequenceEmbeddingColumnTest(test.TestCase):

  def test_get_sequence_dense_tensor(self):
    vocabulary_size = 3
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, ids [2]
        # example 1, ids [0, 1]
        # example 2, ids []
        # example 3, ids [1]
        indices=((0, 0), (1, 0), (1, 1), (3, 0)),
        values=(2, 0, 1, 1),
        dense_shape=(4, 2))

    embedding_dimension = 2
    embedding_values = (
        (1., 2.),  # id 0
        (3., 5.),  # id 1
        (7., 11.)  # id 2
    )
    def _initializer(shape, dtype, partition_info):
      self.assertAllEqual((vocabulary_size, embedding_dimension), shape)
      self.assertEqual(dtypes.float32, dtype)
      self.assertIsNone(partition_info)
      return embedding_values

    expected_lookups = [
        # example 0, ids [2]
        [[7., 11.], [0., 0.]],
        # example 1, ids [0, 1]
        [[1., 2.], [3., 5.]],
        # example 2, ids []
        [[0., 0.], [0., 0.]],
        # example 3, ids [1]
        [[3., 5.], [0., 0.]],
    ]

    categorical_column = sfc.sequence_categorical_column_with_identity(
        key='aaa', num_buckets=vocabulary_size)
    embedding_column = sfc._sequence_embedding_column(
        categorical_column, dimension=embedding_dimension,
        initializer=_initializer)

    embedding_lookup, _ = embedding_column._get_sequence_dense_tensor(
        _LazyBuilder({'aaa': sparse_input}))

    global_vars = ops.get_collection(ops.GraphKeys.GLOBAL_VARIABLES)
    self.assertItemsEqual(
        ('embedding_weights:0',), tuple([v.name for v in global_vars]))
    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(embedding_values, global_vars[0].eval(session=sess))
      self.assertAllEqual(expected_lookups, embedding_lookup.eval(session=sess))

  def test_sequence_length(self):
    vocabulary_size = 3
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, ids [2]
        # example 1, ids [0, 1]
        indices=((0, 0), (1, 0), (1, 1)),
        values=(2, 0, 1),
        dense_shape=(2, 2))
    expected_sequence_length = [1, 2]

    categorical_column = sfc.sequence_categorical_column_with_identity(
        key='aaa', num_buckets=vocabulary_size)
    embedding_column = sfc._sequence_embedding_column(
        categorical_column, dimension=2)

    _, sequence_length = embedding_column._get_sequence_dense_tensor(
        _LazyBuilder({'aaa': sparse_input}))

    with monitored_session.MonitoredSession() as sess:
      sequence_length = sess.run(sequence_length)
      self.assertAllEqual(expected_sequence_length, sequence_length)
      self.assertEqual(np.int64, sequence_length.dtype)

  def test_sequence_length_with_empty_rows(self):
    """Tests _sequence_length when some examples do not have ids."""
    vocabulary_size = 3
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, ids []
        # example 1, ids [2]
        # example 2, ids [0, 1]
        # example 3, ids []
        # example 4, ids [1]
        # example 5, ids []
        indices=((1, 0), (2, 0), (2, 1), (4, 0)),
        values=(2, 0, 1, 1),
        dense_shape=(6, 2))
    expected_sequence_length = [0, 1, 2, 0, 1, 0]

    categorical_column = sfc.sequence_categorical_column_with_identity(
        key='aaa', num_buckets=vocabulary_size)
    embedding_column = sfc._sequence_embedding_column(
        categorical_column, dimension=2)

    _, sequence_length = embedding_column._get_sequence_dense_tensor(
        _LazyBuilder({'aaa': sparse_input}))

    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(
          expected_sequence_length, sequence_length.eval(session=sess))


class SequenceIndicatorColumnTest(test.TestCase):

  def test_get_sequence_dense_tensor(self):
    vocabulary_size = 3
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, ids [2]
        # example 1, ids [0, 1]
        # example 2, ids []
        # example 3, ids [1]
        indices=((0, 0), (1, 0), (1, 1), (3, 0)),
        values=(2, 0, 1, 1),
        dense_shape=(4, 2))

    expected_lookups = [
        # example 0, ids [2]
        [[0., 0., 1.], [0., 0., 0.]],
        # example 1, ids [0, 1]
        [[1., 0., 0.], [0., 1., 0.]],
        # example 2, ids []
        [[0., 0., 0.], [0., 0., 0.]],
        # example 3, ids [1]
        [[0., 1., 0.], [0., 0., 0.]],
    ]

    categorical_column = sfc.sequence_categorical_column_with_identity(
        key='aaa', num_buckets=vocabulary_size)
    indicator_column = sfc._sequence_indicator_column(categorical_column)

    indicator_tensor, _ = indicator_column._get_sequence_dense_tensor(
        _LazyBuilder({'aaa': sparse_input}))

    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(expected_lookups, indicator_tensor.eval(session=sess))

  def test_sequence_length(self):
    vocabulary_size = 3
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, ids [2]
        # example 1, ids [0, 1]
        indices=((0, 0), (1, 0), (1, 1)),
        values=(2, 0, 1),
        dense_shape=(2, 2))
    expected_sequence_length = [1, 2]

    categorical_column = sfc.sequence_categorical_column_with_identity(
        key='aaa', num_buckets=vocabulary_size)
    indicator_column = sfc._sequence_indicator_column(categorical_column)

    _, sequence_length = indicator_column._get_sequence_dense_tensor(
        _LazyBuilder({'aaa': sparse_input}))

    with monitored_session.MonitoredSession() as sess:
      sequence_length = sess.run(sequence_length)
      self.assertAllEqual(expected_sequence_length, sequence_length)
      self.assertEqual(np.int64, sequence_length.dtype)

  def test_sequence_length_with_empty_rows(self):
    """Tests _sequence_length when some examples do not have ids."""
    vocabulary_size = 3
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, ids []
        # example 1, ids [2]
        # example 2, ids [0, 1]
        # example 3, ids []
        # example 4, ids [1]
        # example 5, ids []
        indices=((1, 0), (2, 0), (2, 1), (4, 0)),
        values=(2, 0, 1, 1),
        dense_shape=(6, 2))
    expected_sequence_length = [0, 1, 2, 0, 1, 0]

    categorical_column = sfc.sequence_categorical_column_with_identity(
        key='aaa', num_buckets=vocabulary_size)
    indicator_column = sfc._sequence_indicator_column(categorical_column)

    _, sequence_length = indicator_column._get_sequence_dense_tensor(
        _LazyBuilder({'aaa': sparse_input}))

    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(
          expected_sequence_length, sequence_length.eval(session=sess))


class SequenceNumericColumnTest(test.TestCase):

  def test_get_sequence_dense_tensor(self):
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, values [[0.], [1]]
        # example 1, [[10.]]
        indices=((0, 0), (0, 1), (1, 0)),
        values=(0., 1., 10.),
        dense_shape=(2, 2))
    expected_dense_tensor = [
        [[0.], [1.]],
        [[10.], [0.]],
    ]
    numeric_column = sfc.sequence_numeric_column('aaa')

    dense_tensor, _ = numeric_column._get_sequence_dense_tensor(
        _LazyBuilder({'aaa': sparse_input}))

    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(
          expected_dense_tensor, dense_tensor.eval(session=sess))

  def test_get_sequence_dense_tensor_with_shape(self):
    """Tests get_sequence_dense_tensor with shape !=(1,)."""
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, values [[0., 1., 2.], [3., 4., 5.]]
        # example 1, [[10., 11., 12.]]
        indices=((0, 0), (0, 1), (0, 2), (0, 3), (0, 4), (0, 5),
                 (1, 0), (1, 1), (1, 2)),
        values=(0., 1., 2., 3., 4., 5., 10., 11., 12.),
        dense_shape=(2, 6))
    expected_dense_tensor = [
        [[0., 1., 2.], [3., 4., 5.]],
        [[10., 11., 12.], [0., 0., 0.]],
    ]
    numeric_column = sfc.sequence_numeric_column('aaa', shape=(3,))

    dense_tensor, _ = numeric_column._get_sequence_dense_tensor(
        _LazyBuilder({'aaa': sparse_input}))

    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(
          expected_dense_tensor, dense_tensor.eval(session=sess))

  def test_get_dense_tensor_multi_dim(self):
    """Tests get_sequence_dense_tensor for multi-dim numeric_column."""
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, values [[[0., 1.],  [2., 3.]], [[4., 5.],  [6., 7.]]]
        # example 1, [[[10., 11.],  [12., 13.]]]
        indices=((0, 0), (0, 1), (0, 2), (0, 3), (0, 4), (0, 5), (0, 6), (0, 7),
                 (1, 0), (1, 1), (1, 2), (1, 3)),
        values=(0., 1., 2., 3., 4., 5., 6., 7., 10., 11., 12., 13.),
        dense_shape=(2, 8))
    expected_dense_tensor = [
        [[[0., 1.], [2., 3.]], [[4., 5.], [6., 7.]]],
        [[[10., 11.], [12., 13.]], [[0., 0.], [0., 0.]]],
    ]
    numeric_column = sfc.sequence_numeric_column('aaa', shape=(2, 2))

    dense_tensor, _ = numeric_column._get_sequence_dense_tensor(
        _LazyBuilder({'aaa': sparse_input}))

    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(
          expected_dense_tensor, dense_tensor.eval(session=sess))

  def test_sequence_length(self):
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, values [[0., 1., 2.], [3., 4., 5.]]
        # example 1, [[10., 11., 12.]]
        indices=((0, 0), (0, 1), (0, 2), (0, 3), (0, 4), (0, 5),
                 (1, 0), (1, 1), (1, 2)),
        values=(0., 1., 2., 3., 4., 5., 10., 11., 12.),
        dense_shape=(2, 6))
    expected_sequence_length = [2, 1]
    numeric_column = sfc.sequence_numeric_column('aaa', shape=(3,))

    _, sequence_length = numeric_column._get_sequence_dense_tensor(
        _LazyBuilder({'aaa': sparse_input}))

    with monitored_session.MonitoredSession() as sess:
      sequence_length = sess.run(sequence_length)
      self.assertAllEqual(expected_sequence_length, sequence_length)
      self.assertEqual(np.int64, sequence_length.dtype)

  def test_sequence_length_with_shape(self):
    """Tests _sequence_length with shape !=(1,)."""
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, values [[0.], [1]]
        # example 1, [[10.]]
        indices=((0, 0), (0, 1), (1, 0)),
        values=(0., 1., 10.),
        dense_shape=(2, 2))
    expected_sequence_length = [2, 1]
    numeric_column = sfc.sequence_numeric_column('aaa')

    _, sequence_length = numeric_column._get_sequence_dense_tensor(
        _LazyBuilder({'aaa': sparse_input}))

    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(
          expected_sequence_length, sequence_length.eval(session=sess))

  def test_sequence_length_with_empty_rows(self):
    """Tests _sequence_length when some examples do not have ids."""
    sparse_input = sparse_tensor.SparseTensorValue(
        # example 0, values []
        # example 1, values [[0.], [1.]]
        # example 2, [[2.]]
        # example 3, values []
        # example 4, [[3.]]
        # example 5, values []
        indices=((1, 0), (1, 1), (2, 0), (4, 0)),
        values=(0., 1., 2., 3.),
        dense_shape=(6, 2))
    expected_sequence_length = [0, 2, 1, 0, 1, 0]
    numeric_column = sfc.sequence_numeric_column('aaa')

    _, sequence_length = numeric_column._get_sequence_dense_tensor(
        _LazyBuilder({'aaa': sparse_input}))

    with monitored_session.MonitoredSession() as sess:
      self.assertAllEqual(
          expected_sequence_length, sequence_length.eval(session=sess))


if __name__ == '__main__':
  test.main()
