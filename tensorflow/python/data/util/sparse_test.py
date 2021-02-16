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
"""Tests for utilities working with arbitrarily nested structures."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import functools

from absl.testing import parameterized

from tensorflow.python.data.kernel_tests import test_base
from tensorflow.python.data.util import nest
from tensorflow.python.data.util import sparse
from tensorflow.python.framework import combinations
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import ops
from tensorflow.python.framework import sparse_tensor
from tensorflow.python.framework import tensor_shape
from tensorflow.python.platform import test


# NOTE(vikoth18): Arguments of parameterized tests are lifted into lambdas to make
# sure they are not executed before the (eager- or graph-mode) test environment
# has been set up.
#

def _test_any_sparse_combinations():

  cases = [
      ("CASE_0", lambda: (), False),
      ("CASE_1", lambda: (ops.Tensor), False),
      ("CASE_2", lambda: (((ops.Tensor))), False),
      ("CASE_3", lambda: (ops.Tensor, ops.Tensor), False),
      ("CASE_4", lambda: (ops.Tensor, sparse_tensor.SparseTensor), True),
      ("CASE_5", lambda: (sparse_tensor.SparseTensor, sparse_tensor.SparseTensor), True),
      ("CASE_6", lambda: (((sparse_tensor.SparseTensor))), True)
  ]
  def reduce_fn(x, y):
    name, classes_fn, expected = y
    return x + combinations.combine(
        classes_fn=combinations.NamedObject(
            "classes_fn.{}".format(name), classes_fn
        ),
        expected=combinations.NamedObject(
            "expected.{}".format(name), expected
        )
    )

  return functools.reduce(reduce_fn, cases, [])

def _test_as_dense_shapes_combinations():

  cases = [
      (
          "CASE_0",
          lambda: (),
          lambda: (),
          lambda: ()
      ),
      (
          "CASE_1",
          lambda: tensor_shape.TensorShape([]),
          lambda: ops.Tensor,
          lambda: tensor_shape.TensorShape([])
      ),
      (
          "CASE_2",
          lambda: tensor_shape.TensorShape([]),
          lambda: sparse_tensor.SparseTensor,
          lambda: tensor_shape.unknown_shape() # pylint: disable=unnecessary-lambda
      ),
      (
          "CASE_3",
          lambda: (tensor_shape.TensorShape([])),
          lambda: (ops.Tensor),
          lambda: (tensor_shape.TensorShape([]))
      ),
      (
          "CASE_4",
          lambda: (tensor_shape.TensorShape([])),
          lambda: (sparse_tensor.SparseTensor),
          lambda: (tensor_shape.unknown_shape()) # pylint: disable=unnecessary-lambda
      ),
      (
          "CASE_5",
          lambda: (tensor_shape.TensorShape([]), ()),
          lambda: (ops.Tensor, ()),
          lambda: (tensor_shape.TensorShape([]), ())
      ),
      (
          "CASE_6",
          lambda: ((), tensor_shape.TensorShape([])),
          lambda: ((), ops.Tensor),
          lambda: ((), tensor_shape.TensorShape([]))
      ),
      (
          "CASE_7",
          lambda: (tensor_shape.TensorShape([]), ()),
          lambda: (sparse_tensor.SparseTensor, ()),
          lambda: (tensor_shape.unknown_shape(), ())
      ),
      (
          "CASE_8",
          lambda: ((), tensor_shape.TensorShape([])),
          lambda: ((), sparse_tensor.SparseTensor),
          lambda: ((), tensor_shape.unknown_shape())
      ),
      (
          "CASE_9",
          lambda: (tensor_shape.TensorShape([]), (),
                   tensor_shape.TensorShape([])),
          lambda: (ops.Tensor, (), ops.Tensor),
          lambda: (tensor_shape.TensorShape([]), (),
                   tensor_shape.TensorShape([]))
      ),
      (
          "CASE_10",
          lambda: (tensor_shape.TensorShape([]), (),
                   tensor_shape.TensorShape([])),
          lambda: (sparse_tensor.SparseTensor, (),
                   sparse_tensor.SparseTensor),
          lambda: (tensor_shape.unknown_shape(), (),
                   tensor_shape.unknown_shape())
      ),
      (
          "CASE_11",
          lambda: ((), tensor_shape.TensorShape([]), ()),
          lambda: ((), ops.Tensor, ()),
          lambda: ((), tensor_shape.TensorShape([]), ())
      ),
      (
          "CASE_12",
          lambda: ((), tensor_shape.TensorShape([]), ()),
          lambda: ((), sparse_tensor.SparseTensor, ()),
          lambda: ((), tensor_shape.unknown_shape(), ())
      )
  ]
  def reduce_fn(x, y):
    name, types_fn, classes_fn, expected_fn = y
    return x + combinations.combine(
        types_fn=combinations.NamedObject(
            "types_fn.{}".format(name), types_fn
        ),
        classes_fn=combinations.NamedObject(
            "classes_fn.{}".format(name), classes_fn
        ),
        expected_fn=combinations.NamedObject(
            "expected_fn.{}".format(name), expected_fn
        )
    )

  return functools.reduce(reduce_fn, cases, [])


def _test_as_dense_types_combinations():
  cases = [
      (
          "CASE_0",
          lambda: (),
          lambda: (),
          lambda: ()
      ),
      (
          "CASE_1",
          lambda: dtypes.int32,
          lambda: ops.Tensor,
          lambda: dtypes.int32
      ),
      (
          "CASE_2",
          lambda: dtypes.int32,
          lambda: sparse_tensor.SparseTensor,
          lambda: dtypes.variant
      ),
      (
          "CASE_3",
          lambda: (dtypes.int32),
          lambda: (ops.Tensor),
          lambda: (dtypes.int32)
      ),
      (
          "CASE_4",
          lambda: (dtypes.int32),
          lambda: (sparse_tensor.SparseTensor),
          lambda: (dtypes.variant)
      ),
      (
          "CASE_5",
          lambda: (dtypes.int32, ()),
          lambda: (ops.Tensor, ()),
          lambda: (dtypes.int32, ())
      ),
      (
          "CASE_6",
          lambda: ((), dtypes.int32),
          lambda: ((), ops.Tensor),
          lambda: ((), dtypes.int32)
      ),
      (
          "CASE_7",
          lambda: (dtypes.int32, ()),
          lambda: (sparse_tensor.SparseTensor, ()),
          lambda: (dtypes.variant, ())
      ),
      (
          "CASE_8",
          lambda: ((), dtypes.int32),
          lambda: ((), sparse_tensor.SparseTensor),
          lambda: ((), dtypes.variant)
      ),
      (
          "CASE_9",
          lambda: (dtypes.int32, (), dtypes.int32),
          lambda: (ops.Tensor, (), ops.Tensor),
          lambda: (dtypes.int32, (), dtypes.int32)
      ),
      (
          "CASE_10",
          lambda: (dtypes.int32, (), dtypes.int32),
          lambda: (sparse_tensor.SparseTensor, (),
                   sparse_tensor.SparseTensor),
          lambda: (dtypes.variant, (), dtypes.variant)
      ),
      (
          "CASE_11",
          lambda: ((), dtypes.int32, ()),
          lambda: ((), ops.Tensor, ()),
          lambda: ((), dtypes.int32, ())
      ),
      (
          "CASE_12",
          lambda: ((), dtypes.int32, ()),
          lambda: ((), sparse_tensor.SparseTensor, ()),
          lambda: ((), dtypes.variant, ())
      ),
  ]
  def reduce_fn(x, y):
    name, types_fn, classes_fn, expected_fn = y
    return x + combinations.combine(
        types_fn=combinations.NamedObject(
            "types_fn.{}".format(name), types_fn
        ),
        classes_fn=combinations.NamedObject(
            "classes_fn.{}".format(name), classes_fn
        ),
        expected_fn=combinations.NamedObject(
            "expected_fn.{}".format(name), expected_fn
        )
    )

  return functools.reduce(reduce_fn, cases, [])

def _test_get_classes_combinations():
  cases = [
      (
          "CASE_0",
          lambda: (),
          lambda: ()
      ),
      (
          "CASE_1",
          lambda: sparse_tensor.SparseTensor(
              indices=[[0]], values=[1], dense_shape=[1]),
          lambda: sparse_tensor.SparseTensor
      ),
      (
          "CASE_2",
          lambda: constant_op.constant([1]),
          lambda: ops.Tensor
      ),
      (
          "CASE_3",
          lambda: (sparse_tensor.SparseTensor(
              indices=[[0]], values=[1], dense_shape=[1])),
          lambda: (sparse_tensor.SparseTensor)
      ),
      (
          "CASE_4",
          lambda: (constant_op.constant([1])),
          lambda: (ops.Tensor)
      ),
      (
          "CASE_5",
          lambda: (sparse_tensor.SparseTensor(
              indices=[[0]], values=[1], dense_shape=[1]), ()),
          lambda: (sparse_tensor.SparseTensor, ())
      ),
      (
          "CASE_6",
          lambda: ((), sparse_tensor.SparseTensor(
              indices=[[0]], values=[1], dense_shape=[1])),
          lambda: ((), sparse_tensor.SparseTensor)
      ),
      (
          "CASE_7",
          lambda: (constant_op.constant([1]), ()),
          lambda: (ops.Tensor, ())
      ),
      (
          "CASE_8",
          lambda: ((), constant_op.constant([1])),
          lambda: ((), ops.Tensor)
      ),
      (
          "CASE_9",
          lambda: (sparse_tensor.SparseTensor(
              indices=[[0]], values=[1], dense_shape=[1]),
                   (), constant_op.constant([1])),
          lambda: (sparse_tensor.SparseTensor, (), ops.Tensor)
      ),
      (
          "CASE_10",
          lambda: ((), sparse_tensor.SparseTensor(
              indices=[[0]], values=[1], dense_shape=[1]), ()),
          lambda: ((), sparse_tensor.SparseTensor, ())
      ),
      (
          "CASE_11",
          lambda: ((), constant_op.constant([1]), ()),
          lambda: ((), ops.Tensor, ())
      ),
  ]
  def reduce_fn(x, y):
    name, classes_fn, expected_fn = y
    return x + combinations.combine(
        classes_fn=combinations.NamedObject(
            "classes_fn.{}".format(name), classes_fn
        ),
        expected_fn=combinations.NamedObject(
            "expected_fn.{}".format(name), expected_fn
        )
    )

  return functools.reduce(reduce_fn, cases, [])


def _test_serialize_deserialize_combinations():
  cases = [
      ("CASE_0", lambda: ()),
      ("CASE_1", lambda: sparse_tensor.SparseTensor(
          indices=[[0, 0]], values=[1], dense_shape=[1, 1])),
      ("CASE_2", lambda: sparse_tensor.SparseTensor(
          indices=[[3, 4]], values=[-1], dense_shape=[4, 5])),
      ("CASE_3", lambda: sparse_tensor.SparseTensor(
          indices=[[0, 0], [3, 4]], values=[1, -1],
          dense_shape=[4, 5])),
      ("CASE_4", lambda: (sparse_tensor.SparseTensor(
          indices=[[0, 0]], values=[1], dense_shape=[1, 1]))),
      ("CASE_5", lambda: (sparse_tensor.SparseTensor(
          indices=[[0, 0]], values=[1], dense_shape=[1, 1]), ())),
      ("CASE_6", lambda: ((), sparse_tensor.SparseTensor(
          indices=[[0, 0]], values=[1], dense_shape=[1, 1])))
  ]
  def reduce_fn(x, y):
    name, input_fn = y
    return x + combinations.combine(
        input_fn=combinations.NamedObject(
            "input_fn.{}".format(name), input_fn
        )
    )

  return functools.reduce(reduce_fn, cases, [])


def _test_serialize_many_deserialize_combinations():
  cases = [
      ("CASE_0", lambda: ()),
      ("CASE_1", lambda: sparse_tensor.SparseTensor(
          indices=[[0, 0]], values=[1], dense_shape=[1, 1])),
      ("CASE_2", lambda: sparse_tensor.SparseTensor(
          indices=[[3, 4]], values=[-1], dense_shape=[4, 5])),
      ("CASE_3", lambda: sparse_tensor.SparseTensor(
          indices=[[0, 0], [3, 4]], values=[1, -1],
          dense_shape=[4, 5])),
      ("CASE_4", lambda: (sparse_tensor.SparseTensor(
          indices=[[0, 0]], values=[1], dense_shape=[1, 1]))),
      ("CASE_5", lambda: (sparse_tensor.SparseTensor(
          indices=[[0, 0]], values=[1], dense_shape=[1, 1]), ())),
      ("CASE_6", lambda: ((), sparse_tensor.SparseTensor(
          indices=[[0, 0]], values=[1], dense_shape=[1, 1])))
  ]
  def reduce_fn(x, y):
    name, input_fn = y
    return x + combinations.combine(
        input_fn=combinations.NamedObject(
            "input_fn.{}".format(name), input_fn
        )
    )

  return functools.reduce(reduce_fn, cases, [])


class SparseTest(test_base.DatasetTestBase, parameterized.TestCase):

  @combinations.generate(
      combinations.times(
          test_base.default_test_combinations(),
          _test_any_sparse_combinations()
      )
  )
  def testAnySparse(self, classes_fn, expected):
    classes = classes_fn._obj() # pylint: disable=protected-access
    expected = expected._obj # pylint: disable=protected-access
    self.assertEqual(sparse.any_sparse(classes), expected)

  def assertShapesEqual(self, a, b):
    for a, b in zip(nest.flatten(a), nest.flatten(b)):
      self.assertEqual(a.ndims, b.ndims)
      if a.ndims is None:
        continue
      for c, d in zip(a.as_list(), b.as_list()):
        self.assertEqual(c, d)

  @combinations.generate(
      combinations.times(
          test_base.default_test_combinations(),
          _test_as_dense_shapes_combinations()
      )
  )
  def testAsDenseShapes(self, types_fn, classes_fn, expected_fn):
    types = types_fn._obj() # pylint: disable=protected-access
    classes = classes_fn._obj() # pylint: disable=protected-access
    expected = expected_fn._obj() # pylint: disable=protected-access
    self.assertShapesEqual(sparse.as_dense_shapes(types, classes), expected)

  @combinations.generate(
      combinations.times(
          test_base.default_test_combinations(),
          _test_as_dense_types_combinations()
      )
  )
  def testAsDenseTypes(self, types_fn, classes_fn, expected_fn):
    types = types_fn._obj() # pylint: disable=protected-access
    classes = classes_fn._obj() # pylint: disable=protected-access
    expected = expected_fn._obj() # pylint: disable=protected-access
    self.assertEqual(sparse.as_dense_types(types, classes), expected)

  @combinations.generate(
      combinations.times(
          test_base.default_test_combinations(),
          _test_get_classes_combinations()
      )
  )
  def testGetClasses(self, classes_fn, expected_fn):
    classes = classes_fn._obj() # pylint: disable=protected-access
    expected = expected_fn._obj() # pylint: disable=protected-access
    self.assertEqual(sparse.get_classes(classes), expected)

  def assertSparseValuesEqual(self, a, b):
    if not isinstance(a, sparse_tensor.SparseTensor):
      self.assertFalse(isinstance(b, sparse_tensor.SparseTensor))
      self.assertEqual(a, b)
      return
    self.assertTrue(isinstance(b, sparse_tensor.SparseTensor))
    with self.cached_session():
      self.assertAllEqual(a.eval().indices, self.evaluate(b).indices)
      self.assertAllEqual(a.eval().values, self.evaluate(b).values)
      self.assertAllEqual(a.eval().dense_shape, self.evaluate(b).dense_shape)

  @combinations.generate(
      combinations.times(
          test_base.graph_only_combinations(),
          _test_serialize_deserialize_combinations()
      )
  )
  def testSerializeDeserialize(self, input_fn):
    test_case = input_fn._obj() # pylint: disable=protected-access
    classes = sparse.get_classes(test_case)
    shapes = nest.map_structure(lambda _: tensor_shape.TensorShape(None),
                                classes)
    types = nest.map_structure(lambda _: dtypes.int32, classes)
    actual = sparse.deserialize_sparse_tensors(
        sparse.serialize_sparse_tensors(test_case), types, shapes,
        sparse.get_classes(test_case))
    nest.assert_same_structure(test_case, actual)
    for a, e in zip(nest.flatten(actual), nest.flatten(test_case)):
      self.assertSparseValuesEqual(a, e)

  @combinations.generate(
      combinations.times(
          test_base.graph_only_combinations(),
          _test_serialize_many_deserialize_combinations()
      )
  )
  def testSerializeManyDeserialize(self, input_fn):
    test_case = input_fn._obj() # pylint: disable=protected-access
    classes = sparse.get_classes(test_case)
    shapes = nest.map_structure(lambda _: tensor_shape.TensorShape(None),
                                classes)
    types = nest.map_structure(lambda _: dtypes.int32, classes)
    actual = sparse.deserialize_sparse_tensors(
        sparse.serialize_many_sparse_tensors(test_case), types, shapes,
        sparse.get_classes(test_case))
    nest.assert_same_structure(test_case, actual)
    for a, e in zip(nest.flatten(actual), nest.flatten(test_case)):
      self.assertSparseValuesEqual(a, e)


if __name__ == "__main__":
  test.main()
