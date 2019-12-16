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
"""Math Operations.

Note: Functions taking `Tensor` arguments can also take anything accepted by
`tf.convert_to_tensor`.

Note: Elementwise binary operations in TensorFlow follow [numpy-style
broadcasting](http://docs.scipy.org/doc/numpy/user/basics.broadcasting.html).

TensorFlow provides a variety of math functions including:

* Basic arithmetic operators and trigonometric functions.
* Special math functions (like: `tf.math.igamma` and `tf.math.zeta`)
* Complex number functions (like: `tf.math.imag` and `tf.math.angle`)
* Reductions and scans (like: `tf.math.reduce_mean` and `tf.math.cumsum`)
* Segment functions (like: `tf.math.segment_sum`)

See: `tf.linalg` for matrix and tensor functions.

<a id=Segmentation></a>

## About Segmentation

TensorFlow provides several operations that you can use to perform common
math computations on tensor segments.
Here a segmentation is a partitioning of a tensor along
the first dimension, i.e. it  defines a mapping from the first dimension onto
`segment_ids`. The `segment_ids` tensor should be the size of
the first dimension, `d0`, with consecutive IDs in the range `0` to `k`,
where `k<d0`.
In particular, a segmentation of a matrix tensor is a mapping of rows to
segments.

For example:

```python
c = tf.constant([[1,2,3,4], [-1,-2,-3,-4], [5,6,7,8]])
tf.math.segment_sum(c, tf.constant([0, 0, 1]))
#  ==>  [[0 0 0 0]
#        [5 6 7 8]]
```

The standard `segment_*` functions assert that the segment indices are sorted.
If you have unsorted indices use the equivalent `unsorted_segment_` function.
These functions take an additional argument `num_segments` so that the output
tensor can be efficiently allocated.

``` python
c = tf.constant([[1,2,3,4], [-1,-2,-3,-4], [5,6,7,8]])
tf.math.unsorted_segment_sum(c, tf.constant([0, 1, 0]), num_segments=2)
# ==> [[ 6,  8, 10, 12],
#       [-1, -2, -3, -4]]
```

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np
import six
from six.moves import builtins
from six.moves import xrange  # pylint: disable=redefined-builtin

from tensorflow.python.eager import context
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import graph_util
from tensorflow.python.framework import ops
from tensorflow.python.framework import sparse_tensor
from tensorflow.python.framework import tensor_shape
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import gen_array_ops
from tensorflow.python.ops import gen_data_flow_ops
from tensorflow.python.ops import gen_math_ops
from tensorflow.python.ops import gen_nn_ops
from tensorflow.python.ops import gen_sparse_ops
# go/tf-wildcard-import
# pylint: disable=wildcard-import
from tensorflow.python.ops.gen_math_ops import *
# pylint: enable=wildcard-import
from tensorflow.python.platform import tf_logging as logging
from tensorflow.python.util import compat
from tensorflow.python.util import deprecation
from tensorflow.python.util import dispatch
from tensorflow.python.util import nest
from tensorflow.python.util.tf_export import tf_export

# Aliases for some automatically-generated names.
linspace = gen_math_ops.lin_space
nextafter = gen_math_ops.next_after

arg_max = deprecation.deprecated(None, "Use `tf.math.argmax` instead")(arg_max)  # pylint: disable=used-before-assignment
arg_min = deprecation.deprecated(None, "Use `tf.math.argmin` instead")(arg_min)  # pylint: disable=used-before-assignment
tf_export(v1=["arg_max"])(arg_max)
tf_export(v1=["arg_min"])(arg_min)


# This is set by resource_variable_ops.py. It is included in this way since
# there is a circular dependency between math_ops and resource_variable_ops
_resource_variable_type = None


def _set_doc(doc):

  def _decorator(func):
    func.__doc__ = doc
    return func

  return _decorator


# pylint: disable=redefined-builtin
@tf_export(v1=["math.argmax", "argmax"])
@deprecation.deprecated_args(None, "Use the `axis` argument instead",
                             "dimension")
@_set_doc(
    gen_math_ops.arg_max.__doc__.replace("dimensions",
                                         "axes").replace("dimension", "axis"))
def argmax(input,
           axis=None,
           name=None,
           dimension=None,
           output_type=dtypes.int64):
  axis = deprecation.deprecated_argument_lookup("axis", axis, "dimension",
                                                dimension)
  return argmax_v2(input, axis, output_type, name)


@tf_export("math.argmax", "argmax", v1=[])
def argmax_v2(input, axis=None, output_type=dtypes.int64, name=None):
  """Returns the index with the largest value across axes of a tensor.

  Note that in case of ties the identity of the return value is not guaranteed.

  For example:

  >>> A = tf.constant([2, 20, 30, 3, 6])
  >>> tf.math.argmax(A)  # A[2] is maximum in tensor A
  <tf.Tensor: shape=(), dtype=int64, numpy=2>
  >>> B = tf.constant([[2, 20, 30, 3, 6], [3, 11, 16, 1, 8],
  ...                  [14, 45, 23, 5, 27]])
  >>> tf.math.argmax(B, 0)
  <tf.Tensor: shape=(5,), dtype=int64, numpy=array([2, 2, 0, 2, 2])>
  >>> tf.math.argmax(B, 1)
  <tf.Tensor: shape=(3,), dtype=int64, numpy=array([2, 2, 1])>

  Args:
    input: A `Tensor`.
    axis: An integer, the axis to reduce across. Default to 0.
    output_type: An optional output dtype (`tf.int32` or `tf.int64`). Defaults
      to `tf.int64`.
    name: An optional name for the operation.

  Returns:
    A `Tensor` of type `output_type`.
  """
  if axis is None:
    axis = 0
  return gen_math_ops.arg_max(input, axis, name=name, output_type=output_type)


@tf_export(v1=["math.argmin", "argmin"])
@deprecation.deprecated_args(None, "Use the `axis` argument instead",
                             "dimension")
@_set_doc(
    gen_math_ops.arg_min.__doc__.replace("dimensions",
                                         "axes").replace("dimension", "axis"))
def argmin(input,
           axis=None,
           name=None,
           dimension=None,
           output_type=dtypes.int64):
  axis = deprecation.deprecated_argument_lookup("axis", axis, "dimension",
                                                dimension)
  return argmin_v2(input, axis, output_type, name)


@tf_export("math.argmin", "argmin", v1=[])
def argmin_v2(input, axis=None, output_type=dtypes.int64, name=None):
  """Returns the index with the smallest value across axes of a tensor.

  Note that in case of ties the identity of the return value is not guaranteed.

  Args:
    input: A `Tensor`. Must be one of the following types: `float32`, `float64`,
      `int32`, `uint8`, `int16`, `int8`, `complex64`, `int64`, `qint8`,
      `quint8`, `qint32`, `bfloat16`, `uint16`, `complex128`, `half`, `uint32`,
      `uint64`.
    axis: A `Tensor`. Must be one of the following types: `int32`, `int64`.
      int32 or int64, must be in the range `-rank(input), rank(input))`.
      Describes which axis of the input Tensor to reduce across. For vectors,
      use axis = 0.
    output_type: An optional `tf.DType` from: `tf.int32, tf.int64`. Defaults to
      `tf.int64`.
    name: A name for the operation (optional).

  Returns:
    A `Tensor` of type `output_type`.

  Usage:
  ```python
  import tensorflow as tf
  a = [1, 10, 26.9, 2.8, 166.32, 62.3]
  b = tf.math.argmin(input = a)
  c = tf.keras.backend.eval(b)
  # c = 0
  # here a[0] = 1 which is the smallest element of a across axis 0
  ```
  """
  if axis is None:
    axis = 0
  return gen_math_ops.arg_min(input, axis, name=name, output_type=output_type)


# pylint: enable=redefined-builtin


# pylint: disable=anomalous-backslash-in-string,protected-access
# pylint: disable=g-docstring-has-escape
@tf_export("math.abs", "abs")
@dispatch.add_dispatch_support
def abs(x, name=None):  # pylint: disable=redefined-builtin
  r"""Computes the absolute value of a tensor.

  Given a tensor of integer or floating-point values, this operation returns a
  tensor of the same type, where each element contains the absolute value of the
  corresponding element in the input.

  Given a tensor `x` of complex numbers, this operation returns a tensor of type
  `float32` or `float64` that is the absolute value of each element in `x`. For
  a complex number \\(a + bj\\), its absolute value is computed as \\(\gen_math_ops.sqrt(variance)


@tf_export("math.reduce_prod", "reduce_prod", v1=[])
@dispatch.add_dispatch_support
def reduce_prod(input_tensor, axis=None, keepdims=False, name=None):
  """Computes the product of elements across dimensions of a tensor.

  Reduces `input_tensor` along the dimensions given in `axis`.
  Unless `keepdims` is true, the rank of the tensor is reduced by 1 for each
  entry in `axis`. If `keepdims` is true, the reduced dimensions
  are retained with length 1.

  If `axis` is None, all dimensions are reduced, and a
  tensor with a single element is returned.

  Args:
    input_tensor: The tensor to reduce. Should have numeric type.
    axis: The dimensions to reduce. If `None` (the default), reduces all
      dimensions. Must be in the range `[-rank(input_tensor),
      rank(input_tensor))`.
    keepdims: If true, retains reduced dimensions with length 1.
    name: A name for the operation (optional).

  Returns:
    The reduced tensor.

  @compatibility(numpy)
  Equivalent to np.prod
  @end_compatibility
  """
  keepdims = False if keepdims is None else keepdims
  return _may_reduce_to_scalar(
      keepdims, axis,
      gen_math_ops.prod(
          input_tensor, _ReductionDims(input_tensor, axis), keepdims,
          name=name))


@tf_export(v1=["math.reduce_prod", "reduce_prod"])
@deprecation.deprecated_args(None,
                             "keep_dims is deprecated, use keepdims instead",
                             "keep_dims")
def reduce_prod_v1(input_tensor,
                   axis=None,
                   keepdims=None,
                   name=None,
                   reduction_indices=None,
                   keep_dims=None):
  """Computes the product of elements across dimensions of a tensor.

  Reduces `input_tensor` along the dimensions given in `axis`.
  Unless `keepdims` is true, the rank of the tensor is reduced by 1 for each
  entry in `axis`. If `keepdims` is true, the reduced dimensions
  are retained with length 1.

  If `axis` is None, all dimensions are reduced, and a
  tensor with a single element is returned.

  Args:
    input_tensor: The tensor to reduce. Should have numeric type.
    axis: The dimensions to reduce. If `None` (the default), reduces all
      dimensions. Must be in the range `[-rank(input_tensor),
      rank(input_tensor))`.
    keepdims: If true, retains reduced dimensions with length 1.
    name: A name for the operation (optional).
    reduction_indices: The old (deprecated) name for axis.
    keep_dims: Deprecated alias for `keepdims`.

  Returns:
    The reduced tensor.

  @compatibility(numpy)
  Equivalent to np.prod
  @end_compatibility
  """
  axis = deprecation.deprecated_argument_lookup("axis", axis,
                                                "reduction_indices",
                                                reduction_indices)
  keepdims = deprecation.deprecated_argument_lookup("keepdims", keepdims,
                                                    "keep_dims", keep_dims)
  return reduce_prod(input_tensor, axis, keepdims, name)


@tf_export(v1=["math.reduce_min", "reduce_min"])
@deprecation.deprecated_args(None,
                             "keep_dims is deprecated, use keepdims instead",
                             "keep_dims")
def reduce_min_v1(input_tensor,
                  axis=None,
                  keepdims=None,
                  name=None,
                  reduction_indices=None,
                  keep_dims=None):
  """Computes the minimum of elements across dimensions of a tensor.

  Reduces `input_tensor` along the dimensions given in `axis`.
  Unless `keepdims` is true, the rank of the tensor is reduced by 1 for each
  entry in `axis`. If `keepdims` is true, the reduced dimensions
  are retained with length 1.

  If `axis` is None, all dimensions are reduced, and a
  tensor with a single element is returned.

  Args:
    input_tensor: The tensor to reduce. Should have real numeric type.
    axis: The dimensions to reduce. If `None` (the default), reduces all
      dimensions. Must be in the range `[-rank(input_tensor),
      rank(input_tensor))`.
    keepdims: If true, retains reduced dimensions with length 1.
    name: A name for the operation (optional).
    reduction_indices: The old (deprecated) name for axis.
    keep_dims: Deprecated alias for `keepdims`.

  Returns:
    The reduced tensor.

  @compatibility(numpy)
  Equivalent to np.min
  @end_compatibility
  """
  axis = deprecation.deprecated_argument_lookup("axis", axis,
                                                "reduction_indices",
                                                reduction_indices)
  keepdims = deprecation.deprecated_argument_lookup("keepdims", keepdims,
                                                    "keep_dims", keep_dims)
  return reduce_min(input_tensor, axis, keepdims, name)


@tf_export("math.reduce_min", "reduce_min", v1=[])
@dispatch.add_dispatch_support
def reduce_min(input_tensor, axis=None, keepdims=False, name=None):
  """Computes the minimum of elements across dimensions of a tensor.

  Reduces `input_tensor` along the dimensions given in `axis`.
  Unless `keepdims` is true, the rank of the tensor is reduced by 1 for each
  entry in `axis`. If `keepdims` is true, the reduced dimensions
  are retained with length 1.

  If `axis` is None, all dimensions are reduced, and a
  tensor with a single element is returned.

  Args:
    input_tensor: The tensor to reduce. Should have real numeric type.
    axis: The dimensions to reduce. If `None` (the default), reduces all
      dimensions. Must be in the range `[-rank(input_tensor),
      rank(input_tensor))`.
    keepdims: If true, retains reduced dimensions with length 1.
    name: A name for the operation (optional).

  Returns:
    The reduced tensor.

  @compatibility(numpy)
  Equivalent to np.min
  @end_compatibility
  """
  keepdims = False if keepdims is None else keepdims
  return _may_reduce_to_scalar(
      keepdims, axis,
      gen_math_ops._min(
          input_tensor, _ReductionDims(input_tensor, axis), keepdims,
          name=name))


@tf_export(v1=["math.reduce_max", "reduce_max"])
@deprecation.deprecated_args(None,
                             "keep_dims is deprecated, use keepdims instead",
                             "keep_dims")
def reduce_max_v1(input_tensor,
                  axis=None,
                  keepdims=None,
                  name=None,
                  reduction_indices=None,
                  keep_dims=None):
  """Computes the maximum of elements across dimensions of a tensor.

  Reduces `input_tensor` along the dimensions given in `axis`.
  Unless `keepdims` is true, the rank of the tensor is reduced by 1 for each
  entry in `axis`. If `keepdims` is true, the reduced dimensions
  are retained with length 1.

  If `axis` is None, all dimensions are reduced, and a
  tensor with a single element is returned.

  Args:
    input_tensor: The tensor to reduce. Should have real numeric type.
    axis: The dimensions to reduce. If `None` (the default), reduces all
      dimensions. Must be in the range `[-rank(input_tensor),
      rank(input_tensor))`.
    keepdims: If true, retains reduced dimensions with length 1.
    name: A name for the operation (optional).
    reduction_indices: The old (deprecated) name for axis.
    keep_dims: Deprecated alias for `keepdims`.

  Returns:
    The reduced tensor.

  @compatibility(numpy)
  Equivalent to np.max
  @end_compatibility
  """
  axis = deprecation.deprecated_argument_lookup("axis", axis,
                                                "reduction_indices",
                                                reduction_indices)
  keepdims = deprecation.deprecated_argument_lookup("keepdims", keepdims,
                                                    "keep_dims", keep_dims)
  return reduce_max(input_tensor, axis, keepdims, name)


@tf_export("math.reduce_max", "reduce_max", v1=[])
@dispatch.add_dispatch_support
def reduce_max(input_tensor, axis=None, keepdims=False, name=None):
  """Computes the maximum of elements across dimensions of a tensor.

  Reduces `input_tensor` along the dimensions given in `axis`.
  Unless `keepdims` is true, the rank of the tensor is reduced by 1 for each
  entry in `axis`. If `keepdims` is true, the reduced dimensions
  are retained with length 1.

  If `axis` is None, all dimensions are reduced, and a
  tensor with a single element is returned.

  Args:
    input_tensor: The tensor to reduce. Should have real numeric type.
    axis: The dimensions to reduce. If `None` (the default), reduces all
      dimensions. Must be in the range `[-rank(input_tensor),
      rank(input_tensor))`.
    keepdims: If true, retains reduced dimensions with length 1.
    name: A name for the operation (optional).

  Returns:
    The reduced tensor.

  @compatibility(numpy)
  Equivalent to np.max
  @end_compatibility
  """
  return reduce_max_with_dims(input_tensor, axis, keepdims, name,
                              _ReductionDims(input_tensor, axis))


def reduce_max_with_dims(input_tensor,
                         axis=None,
                         keepdims=False,
                         name=None,
                         dims=None):
  keepdims = False if keepdims is None else keepdims
  return _may_reduce_to_scalar(
      keepdims, axis,
      gen_math_ops._max(input_tensor, dims, keepdims, name=name))


@tf_export(v1=["math.reduce_all", "reduce_all"])
@deprecation.deprecated_args(None,
                             "keep_dims is deprecated, use keepdims instead",
                             "keep_dims")
def reduce_all_v1(input_tensor,
                  axis=None,
                  keepdims=None,
                  name=None,
                  reduction_indices=None,
                  keep_dims=None):
  """Computes the "logical and" of elements across dimensions of a tensor.

  Reduces `input_tensor` along the dimensions given in `axis`.
  Unless `keepdims` is true, the rank of the tensor is reduced by 1 for each
  entry in `axis`. If `keepdims` is true, the reduced dimensions
  are retained with length 1.

  If `axis` is None, all dimensions are reduced, and a
  tensor with a single element is returned.

  For example:

  ```python
  x = tf.constant([[True,  True], [False, False]])
  tf.reduce_all(x)  # False
  tf.reduce_all(x, 0)  # [False, False]
  tf.reduce_all(x, 1)  # [True, False]
  ```

  Args:
    input_tensor: The boolean tensor to reduce.
    axis: The dimensions to reduce. If `None` (the default), reduces all
      dimensions. Must be in the range `[-rank(input_tensor),
      rank(input_tensor))`.
    keepdims: If true, retains reduced dimensions with length 1.
    name: A name for the operation (optional).
    reduction_indices: The old (deprecated) name for axis.
    keep_dims: Deprecated alias for `keepdims`.

  Returns:
    The reduced tensor.

  @compatibility(numpy)
  Equivalent to np.all
  @end_compatibility
  """
  axis = deprecation.deprecated_argument_lookup("axis", axis,
                                                "reduction_indices",
                                                reduction_indices)
  keepdims = deprecation.deprecated_argument_lookup("keepdims", keepdims,
                                                    "keep_dims", keep_dims)
  return reduce_all(input_tensor, axis, keepdims, name)


@tf_export("reduce_all", "math.reduce_all", v1=[])
@dispatch.add_dispatch_support
def reduce_all(input_tensor, axis=None, keepdims=False, name=None):
  """Computes the "logical and" of elements across dimensions of a tensor.

  Reduces `input_tensor` along the dimensions given in `axis`.
  Unless `keepdims` is true, the rank of the tensor is reduced by 1 for each
  entry in `axis`. If `keepdims` is true, the reduced dimensions
  are retained with length 1.

  If `axis` is None, all dimensions are reduced, and a
  tensor with a single element is returned.

  For example:

  ```python
  x = tf.constant([[True,  True], [False, False]])
  tf.reduce_all(x)  # False
  tf.reduce_all(x, 0)  # [False, False]
  tf.reduce_all(x, 1)  # [True, False]
  ```

  Args:
    input_tensor: The boolean tensor to reduce.
    axis: The dimensions to reduce. If `None` (the default), reduces all
      dimensions. Must be in the range `[-rank(input_tensor),
      rank(input_tensor))`.
    keepdims: If true, retains reduced dimensions with length 1.
    name: A name for the operation (optional).

  Returns:
    The reduced tensor.

  @compatibility(numpy)
  Equivalent to np.all
  @end_compatibility
  """
  keepdims = False if keepdims is None else keepdims
  return _may_reduce_to_scalar(
      keepdims, axis,
      gen_math_ops._all(
          input_tensor, _ReductionDims(input_tensor, axis), keepdims,
          name=name))


@tf_export(v1=["math.reduce_any", "reduce_any"])
@deprecation.deprecated_args(None,
                             "keep_dims is deprecated, use keepdims instead",
                             "keep_dims")
def reduce_any_v1(input_tensor,
                  axis=None,
                  keepdims=None,
                  name=None,
                  reduction_indices=None,
                  keep_dims=None):
  """Computes the "logical or" of elements across dimensions of a tensor.

  Reduces `input_tensor` along the dimensions given in `axis`.
  Unless `keepdims` is true, the rank of the tensor is reduced by 1 for each
  entry in `axis`. If `keepdims` is true, the reduced dimensions
  are retained with length 1.

  If `axis` is None, all dimensions are reduced, and a
  tensor with a single element is returned.

  For example:

  ```python
  x = tf.constant([[True,  True], [False, False]])
  tf.reduce_any(x)  # True
  tf.reduce_any(x, 0)  # [True, True]
  tf.reduce_any(x, 1)  # [True, False]
  ```

  Args:
    input_tensor: The boolean tensor to reduce.
    axis: The dimensions to reduce. If `None` (the default), reduces all
      dimensions. Must be in the range `[-rank(input_tensor),
      rank(input_tensor))`.
    keepdims: If true, retains reduced dimensions with length 1.
    name: A name for the operation (optional).
    reduction_indices: The old (deprecated) name for axis.
    keep_dims: Deprecated alias for `keepdims`.

  Returns:
    The reduced tensor.

  @compatibility(numpy)
  Equivalent to np.any
  @end_compatibility
  """
  axis = deprecation.deprecated_argument_lookup("axis", axis,
                                                "reduction_indices",
                                                reduction_indices)
  keepdims = deprecation.deprecated_argument_lookup("keepdims", keepdims,
                                                    "keep_dims", keep_dims)
  return reduce_any(input_tensor, axis, keepdims, name)


@tf_export("math.reduce_any", "reduce_any", v1=[])
@dispatch.add_dispatch_support
def reduce_any(input_tensor, axis=None, keepdims=False, name=None):
  """Computes the "logical or" of elements across dimensions of a tensor.

  Reduces `input_tensor` along the dimensions given in `axis`.
  Unless `keepdims` is true, the rank of the tensor is reduced by 1 for each
  entry in `axis`. If `keepdims` is true, the reduced dimensions
  are retained with length 1.

  If `axis` is None, all dimensions are reduced, and a
  tensor with a single element is returned.

  For example:

  ```python
  x = tf.constant([[True,  True], [False, False]])
  tf.reduce_any(x)  # True
  tf.reduce_any(x, 0)  # [True, True]
  tf.reduce_any(x, 1)  # [True, False]
  ```

  Args:
    input_tensor: The boolean tensor to reduce.
    axis: The dimensions to reduce. If `None` (the default), reduces all
      dimensions. Must be in the range `[-rank(input_tensor),
      rank(input_tensor))`.
    keepdims: If true, retains reduced dimensions with length 1.
    name: A name for the operation (optional).

  Returns:
    The reduced tensor.

  @compatibility(numpy)
  Equivalent to np.any
  @end_compatibility
  """
  keepdims = False if keepdims is None else keepdims
  return _may_reduce_to_scalar(
      keepdims, axis,
      gen_math_ops._any(
          input_tensor, _ReductionDims(input_tensor, axis), keepdims,
          name=name))


@tf_export(v1=["math.reduce_logsumexp", "reduce_logsumexp"])
@deprecation.deprecated_args(None,
                             "keep_dims is deprecated, use keepdims instead",
                             "keep_dims")
def reduce_logsumexp_v1(input_tensor,
                        axis=None,
                        keepdims=None,
                        name=None,
                        reduction_indices=None,
                        keep_dims=None):
  """Computes log(sum(exp(elements across dimensions of a tensor))).

  Reduces `input_tensor` along the dimensions given in `axis`.
  Unless `keepdims` is true, the rank of the tensor is reduced by 1 for each
  entry in `axis`. If `keepdims` is true, the reduced dimensions
  are retained with length 1.

  If `axis` has no entries, all dimensions are reduced, and a
  tensor with a single element is returned.

  This function is more numerically stable than log(sum(exp(input))). It avoids
  overflows caused by taking the exp of large inputs and underflows caused by
  taking the log of small inputs.

  For example:

  ```python
  x = tf.constant([[0., 0., 0.], [0., 0., 0.]])
  tf.reduce_logsumexp(x)  # log(6)
  tf.reduce_logsumexp(x, 0)  # [log(2), log(2), log(2)]
  tf.reduce_logsumexp(x, 1)  # [log(3), log(3)]
  tf.reduce_logsumexp(x, 1, keepdims=True)  # [[log(3)], [log(3)]]
  tf.reduce_logsumexp(x, [0, 1])  # log(6)
  ```

  Args:
    input_tensor: The tensor to reduce. Should have numeric type.
    axis: The dimensions to reduce. If `None` (the default), reduces all
      dimensions. Must be in the range `[-rank(input_tensor),
      rank(input_tensor))`.
    keepdims: If true, retains reduced dimensions with length 1.
    name: A name for the operation (optional).
    reduction_indices: The old (deprecated) name for axis.
    keep_dims: Deprecated alias for `keepdims`.

  Returns:
    The reduced tensor.
  """
  axis = deprecation.deprecated_argument_lookup("axis", axis,
                                                "reduction_indices",
                                                reduction_indices)
  keepdims = deprecation.deprecated_argument_lookup("keepdims", keepdims,
                                                    "keep_dims", keep_dims)
  return reduce_logsumexp(input_tensor, axis, keepdims, name)


@tf_export("math.reduce_logsumexp", "reduce_logsumexp", v1=[])
def reduce_logsumexp(input_tensor, axis=None, keepdims=False, name=None):
  """Computes log(sum(exp(elements across dimensions of a tensor))).

  Reduces `input_tensor` along the dimensions given in `axis`.
  Unless `keepdims` is true, the rank of the tensor is reduced by 1 for each
  entry in `axis`. If `keepdims` is true, the reduced dimensions
  are retained with length 1.

  If `axis` has no entries, all dimensions are reduced, and a
  tensor with a single element is returned.

  This function is more numerically stable than log(sum(exp(input))). It avoids
  overflows caused by taking the exp of large inputs and underflows caused by
  taking the log of small inputs.

  For example:

  ```python
  x = tf.constant([[0., 0., 0.], [0., 0., 0.]])
  tf.reduce_logsumexp(x)  # log(6)
  tf.reduce_logsumexp(x, 0)  # [log(2), log(2), log(2)]
  tf.reduce_logsumexp(x, 1)  # [log(3), log(3)]
  tf.reduce_logsumexp(x, 1, keepdims=True)  # [[log(3)], [log(3)]]
  tf.reduce_logsumexp(x, [0, 1])  # log(6)
  ```

  Args:
    input_tensor: The tensor to reduce. Should have numeric type.
    axis: The dimensions to reduce. If `None` (the default), reduces all
      dimensions. Must be in the range `[-rank(input_tensor),
      rank(input_tensor))`.
    keepdims: If true, retains reduced dimensions with length 1.
    name: A name for the operation (optional).

  Returns:
    The reduced tensor.
  """
  keepdims = False if keepdims is None else keepdims
  input_tensor = ops.convert_to_tensor(input_tensor)
  with ops.name_scope(name, "ReduceLogSumExp", [input_tensor]) as name:
    reduce_dim = _ReductionDims(input_tensor, axis)
    raw_max = reduce_max_with_dims(
        input_tensor, axis=axis, keepdims=True, dims=reduce_dim)
    my_max = array_ops.stop_gradient(
        gen_math_ops.select_v2(
            gen_math_ops.is_finite(raw_max), raw_max,
            gen_array_ops.zeros_like(raw_max)))
    result = gen_math_ops.log(
        reduce_sum_with_dims(
            gen_math_ops.exp(gen_math_ops.sub(input_tensor, my_max)),
            axis=axis,
            keepdims=keepdims,
            dims=reduce_dim))
    if not keepdims:
      my_max = array_ops.reshape(my_max, gen_array_ops.shape(result))
    result = gen_math_ops.add(result, my_max)
    return _may_reduce_to_scalar(keepdims, axis, result)


@tf_export("linalg.trace", v1=["linalg.trace", "trace"])
@deprecation.deprecated_endpoints("trace")
@dispatch.add_dispatch_support
def trace(x, name=None):
  """Compute the trace of a tensor `x`.

  `trace(x)` returns the sum along the main diagonal of each inner-most matrix
  in x. If x is of rank `k` with shape `[I, J, K, ..., L, M, N]`, then output
  is a tensor of rank `k-2` with dimensions `[I, J, K, ..., L]` where

  `output[i, j, k, ..., l] = trace(x[i, j, i, ..., l, :, :])`

  For example:

  ```python
  x = tf.constant([[1, 2], [3, 4]])
  tf.linalg.trace(x)  # 5

  x = tf.constant([[1, 2, 3],
                   [4, 5, 6],
                   [7, 8, 9]])
  tf.linalg.trace(x)  # 15

  x = tf.constant([[[1, 2, 3],
                    [4, 5, 6],
                    [7, 8, 9]],
                   [[-1, -2, -3],
                    [-4, -5, -6],
                    [-7, -8, -9]]])
  tf.linalg.trace(x)  # [15, -15]
  ```

  Args:
    x: tensor.
    name: A name for the operation (optional).

  Returns:
    The trace of input tensor.
  """
  with ops.name_scope(name, "Trace", [x]) as name:
    x = ops.convert_to_tensor(x, name="x")
    return reduce_sum(array_ops.matrix_diag_part(x), [-1], name=name)


@tf_export("linalg.matmul", "matmul")
@dispatch.add_dispatch_support
def matmul(a,
           b,
           transpose_a=False,
           transpose_b=False,
           adjoint_a=False,
           adjoint_b=False,
           a_is_sparse=False,
           b_is_sparse=False,
           name=None):
  """Multiplies matrix `a` by matrix `b`, producing `a` * `b`.

  The inputs must, following any transpositions, be tensors of rank >= 2
  where the inner 2 dimensions specify valid matrix multiplication dimensions,
  and any further outer dimensions specify matching batch size.

  Both matrices must be of the same type. The supported types are:
  `float16`, `float32`, `float64`, `int32`, `complex64`, `complex128`.

  Either matrix can be transposed or adjointed (conjugated and transposed) on
  the fly by setting one of the corresponding flag to `True`. These are `False`
  by default.

  If one or both of the matrices contain a lot of zeros, a more efficient
  multiplication algorithm can be used by setting the corresponding
  `a_is_sparse` or `b_is_sparse` flag to `True`. These are `False` by default.
  This optimization is only available for plain matrices (rank-2 tensors) with
  datatypes `bfloat16` or `float32`.

  A simple 2-D tensor matrix multiplication:
  >>> a = tf.constant([1, 2, 3, 4, 5, 6], shape=[2, 3])
  >>> a  # 2-D tensor
  <tf.Tensor: shape=(2, 3), dtype=int32, numpy=
  array([[1, 2, 3],
         [4, 5, 6]], dtype=int32)>
  >>> b = tf.constant([7, 8, 9, 10, 11, 12], shape=[3, 2])
  >>> b  # 2-D tensor
  <tf.Tensor: shape=(3, 2), dtype=int32, numpy=
  array([[ 7,  8],
         [ 9, 10],
         [11, 12]], dtype=int32)>
  >>> c = tf.matmul(a, b)
  >>> c  # `a` * `b`
  <tf.Tensor: shape=(2, 2), dtype=int32, numpy=
  array([[ 58,  64],
         [139, 154]], dtype=int32)>

  A batch matrix multiplication with batch shape [2]
  >>> a = tf.constant(np.arange(1, 13, dtype=np.int32), shape=[2, 2, 3])
  >>> a  # 3-D tensor
  <tf.Tensor: shape=(2, 2, 3), dtype=int32, numpy=
  array([[[ 1,  2,  3],
          [ 4,  5,  6]],
         [[ 7,  8,  9],
          [10, 11, 12]]], dtype=int32)>
  >>> b = tf.constant(np.arange(13, 25, dtype=np.int32), shape=[2, 3, 2])
  >>> b  # 3-D tensor
  <tf.Tensor: shape=(2, 3, 2), dtype=int32, numpy=
  array([[[13, 14],
          [15, 16],
          [17, 18]],
         [[19, 20],
          [21, 22],
          [23, 24]]], dtype=int32)>
  >>> c = tf.matmul(a, b)
  >>> c  # `a` * `b`
  <tf.Tensor: shape=(2, 2, 2), dtype=int32, numpy=
  array([[[ 94, 100],
          [229, 244]],
         [[508, 532],
          [697, 730]]], dtype=int32)>

  Since python >= 3.5 the @ operator is supported
  (see [PEP 465](https://www.python.org/dev/peps/pep-0465/)). In TensorFlow,
  it simply calls the `tf.matmul()` function, so the following lines are
  equivalent:
  >>> d = a @ b @ [[10], [11]]
  >>> d = tf.matmul(tf.matmul(a, b), [[10], [11]])

  Args:
    a: `tf.Tensor` of type `float16`, `float32`, `float64`, `int32`,
      `complex64`, `complex128` and rank > 1.
    b: `tf.Tensor` with same type and rank as `a`.
    transpose_a: If `True`, `a` is transposed before multiplication.
    transpose_b: If `True`, `b` is transposed before multiplication.
    adjoint_a: If `True`, `a` is conjugated and transposed before
      multiplication.
    adjoint_b: If `True`, `b` is conjugated and transposed before
      multiplication.
    a_is_sparse: If `True`, `a` is treated as a sparse matrix.
    b_is_sparse: If `True`, `b` is treated as a sparse matrix.
    name: Name for the operation (optional).

  Returns:
    A `tf.Tensor` of the same type as `a` and `b` where each inner-most matrix
    is the product of the corresponding matrices in `a` and `b`, e.g. if all
    transpose or adjoint attributes are `False`:

    `output[..., i, j] = sum_k (a[..., i, k] * b[..., k, j])`,
    for all indices `i`, `j`.

    Note: This is matrix product, not element-wise product.


  Raises:
    ValueError: If `transpose_a` and `adjoint_a`, or `transpose_b` and
      `adjoint_b` are both set to `True`.
  """
  with ops.name_scope(name, "MatMul", [a, b]) as name:
    if transpose_a and adjoint_a:
      raise ValueError("Only one of transpose_a and adjoint_a can be True.")
    if transpose_b and adjoint_b:
      raise ValueError("Only one of transpose_b and adjoint_b can be True.")

    if context.executing_eagerly():
      if not isinstance(a, (ops.EagerTensor, _resource_variable_type)):
        a = ops.convert_to_tensor(a, name="a")
      if not isinstance(b, (ops.EagerTensor, _resource_variable_type)):
        b = ops.convert_to_tensor(b, name="b")
    else:
      a = ops.convert_to_tensor(a, name="a")
      b = ops.convert_to_tensor(b, name="b")

    # TODO(apassos) remove _shape_tuple here when it is not needed.
    a_shape = a._shape_tuple()  # pylint: disable=protected-access
    b_shape = b._shape_tuple()  # pylint: disable=protected-access

    output_may_have_non_empty_batch_shape = (
        (a_shape is None or len(a_shape) > 2) or
        (b_shape is None or len(b_shape) > 2))

    if (not a_is_sparse and
        not b_is_sparse) and output_may_have_non_empty_batch_shape:
      # BatchMatmul does not support transpose, so we conjugate the matrix and
      # use adjoint instead. Conj() is a noop for real matrices.
      if transpose_a:
        a = conj(a)
        adjoint_a = True
      if transpose_b:
        b = conj(b)
        adjoint_b = True
      return gen_math_ops.batch_mat_mul_v2(
          a, b, adj_x=adjoint_a, adj_y=adjoint_b, name=name)

    # Neither matmul nor sparse_matmul support adjoint, so we conjugate
    # the matrix and use transpose instead. Conj() is a noop for real
    # matrices.
    if adjoint_a:
      a = conj(a)
      transpose_a = True
    if adjoint_b:
      b = conj(b)
      transpose_b = True

    use_sparse_matmul = False
    if a_is_sparse or b_is_sparse:
      sparse_matmul_types = [dtypes.bfloat16, dtypes.float32]
      use_sparse_matmul = (
          a.dtype in sparse_matmul_types and b.dtype in sparse_matmul_types)
    if ((a.dtype == dtypes.bfloat16 or b.dtype == dtypes.bfloat16) and
        a.dtype != b.dtype):
      # matmul currently doesn't handle mixed-precision inputs.
      use_sparse_matmul = True
    if use_sparse_matmul:
      ret = sparse_matmul(
          a,
          b,
          transpose_a=transpose_a,
          transpose_b=transpose_b,
          a_is_sparse=a_is_sparse,
          b_is_sparse=b_is_sparse,
          name=name)
      # sparse_matmul always returns float32, even with
      # bfloat16 inputs. This prevents us from configuring bfloat16 training.
      # casting to bfloat16 also matches non-sparse matmul behavior better.
      if a.dtype == dtypes.bfloat16 and b.dtype == dtypes.bfloat16:
        ret = cast(ret, dtypes.bfloat16)
      return ret
    else:
      return gen_math_ops.mat_mul(
          a, b, transpose_a=transpose_a, transpose_b=transpose_b, name=name)


@tf_export("linalg.matvec")
def matvec(a,
           b,
           transpose_a=False,
           adjoint_a=False,
           a_is_sparse=False,
           b_is_sparse=False,
           name=None):
  """Multiplies matrix `a` by vector `b`, producing `a` * `b`.

  The matrix `a` must, following any transpositions, be a tensor of rank >= 2,
  with `shape(a)[-1] == shape(b)[-1]`, and `shape(a)[:-2]` able to broadcast
  with `shape(b)[:-1]`.

  Both `a` and `b` must be of the same type. The supported types are:
  `float16`, `float32`, `float64`, `int32`, `complex64`, `complex128`.

  Matrix `a` can be transposed or adjointed (conjugated and transposed) on
  the fly by setting one of the corresponding flag to `True`. These are `False`
  by default.

  If one or both of the inputs contain a lot of zeros, a more efficient
  multiplication algorithm can be used by setting the corresponding
  `a_is_sparse` or `b_is_sparse` flag to `True`. These are `False` by default.
  This optimization is only available for plain matrices/vectors (rank-2/1
  tensors) with datatypes `bfloat16` or `float32`.

  For example:

  ```python
  # 2-D tensor `a`
  # [[1, 2, 3],
  #  [4, 5, 6]]
  a = tf.constant([1, 2, 3, 4, 5, 6], shape=[2, 3])

  # 1-D tensor `b`
  # [7, 9, 11]
  b = tf.constant([7, 9, 11], shape=[3])

  # `a` * `b`
  # [ 58,  64]
  c = tf.linalg.matvec(a, b)


  # 3-D tensor `a`
  # [[[ 1,  2,  3],
  #   [ 4,  5,  6]],
  #  [[ 7,  8,  9],
  #   [10, 11, 12]]]
  a = tf.constant(np.arange(1, 13, dtype=np.int32),
                  shape=[2, 2, 3])

  # 2-D tensor `b`
  # [[13, 14, 15],
  #  [16, 17, 18]]
  b = tf.constant(np.arange(13, 19, dtype=np.int32),
                  shape=[2, 3])

  # `a` * `b`
  # [[ 86, 212],
  #  [410, 563]]
  c = tf.linalg.matvec(a, b)
  ```

  Args:
    a: `Tensor` of type `float16`, `float32`, `float64`, `int32`, `complex64`,
      `complex128` and rank > 1.
    b: `Tensor` with same type as `a` and compatible dimensions.
    transpose_a: If `True`, `a` is transposed before multiplication.
    adjoint_a: If `True`, `a` is conjugated and transposed before
      multiplication.
    a_is_sparse: If `True`, `a` is treated as a sparse matrix.
    b_is_sparse: If `True`, `b` is treated as a sparse matrix.
    name: Name for the operation (optional).

  Returns:
    A `Tensor` of the same type as `a` and `b` where each inner-most vector is
    the product of the corresponding matrices in `a` and vectors in `b`, e.g. if
    all transpose or adjoint attributes are `False`:

    `output`[..., i] = sum_k (`a`[..., i, k] * `b`[..., k]), for all indices i.

    Note: This is matrix-vector product, not element-wise product.


  Raises:
    ValueError: If transpose_a and adjoint_a are both set to True.
  """
  with ops.name_scope(name, "MatVec", [a, b]) as name:
    output = matmul(
        a,
        array_ops.expand_dims(b, axis=-1),
        transpose_a=transpose_a,
        adjoint_a=adjoint_a,
        a_is_sparse=a_is_sparse,
        b_is_sparse=b_is_sparse)
    return array_ops.squeeze(output, axis=-1)


_OverrideBinaryOperatorHelper(matmul, "matmul")

sparse_matmul = deprecation.deprecated(None, "Use `tf.linalg.matmul` instead")(
    gen_math_ops.sparse_mat_mul)
tf_export(v1=["sparse_matmul"])(sparse_matmul)


@ops.RegisterStatistics("MatMul", "flops")
def _calc_mat_mul_flops(graph, node):
  """Calculates the compute resources needed for MatMul."""
  transpose_a = node.attr["transpose_a"].b
  a_shape = graph_util.tensor_shape_from_node_def_name(graph, node.input[0])
  a_shape.assert_is_fully_defined()
  if transpose_a:
    k = int(a_shape[0])
  else:
    k = int(a_shape[1])
  output_shape = graph_util.tensor_shape_from_node_def_name(graph, node.name)
  output_shape.assert_is_fully_defined()
  output_count = np.prod(output_shape.as_list())
  return ops.OpStats("flops", (k * output_count * 2))


@ops.RegisterStatistics("BatchMatMul", "flops")
@ops.RegisterStatistics("BatchMatMulV2", "flops")
def _calc_batch_mat_mul_flops(graph, node):
  """Calculates the compute resources needed for BatchMatMul."""
  transpose_a = node.attr["transpose_a"].b
  a_shape = graph_util.tensor_shape_from_node_def_name(graph, node.input[0])
  a_shape.assert_is_fully_defined()
  if transpose_a:
    k = int(a_shape[-2])
  else:
    k = int(a_shape[-1])
  output_shape = graph_util.tensor_shape_from_node_def_name(graph, node.name)
  output_shape.assert_is_fully_defined()
  output_count = np.prod(output_shape.as_list())
  return ops.OpStats("flops", (k * output_count * 2))


def _as_indexed_slices(x, optimize=True):
  """Convert 'x' to IndexedSlices.

  Convert a dense Tensor to a block-sparse IndexedSlices.

  Args:
    x: Either a Tensor object, or an IndexedSlices object.
    optimize: if true, attempt to optimize the conversion of 'x'.

  Returns:
    An IndexedSlices object.

  Raises:
    TypeError: If 'x' is not a Tensor or an IndexedSlices object.
  """
  # TODO(touts): op_scope
  if not isinstance(x, (ops.Tensor, ops.IndexedSlices)):
    raise TypeError("Not a Tensor or IndexedSlices: %s" % type(x))
  if isinstance(x, ops.IndexedSlices):
    return x
  x_shape = array_ops.shape_internal(x, optimize=optimize)
  return ops.IndexedSlices(x, range(0, x_shape[0]), x_shape)


def _as_indexed_slices_list(inputs, optimize=True):
  """Convert all elements of 'inputs' to IndexedSlices.

  Additionally, homogenize the types of all the indices to
  either int32 or int64.

  Args:
    inputs: List containing either Tensor or IndexedSlices objects.
    optimize: if true, attempt to optimize the conversion of each input.

  Returns:
    A list of IndexedSlices objects.

  Raises:
    TypeError: If 'inputs' is not a list or a tuple.
  """
  if not isinstance(inputs, (list, tuple)):
    raise TypeError("Expected a list or tuple, not a %s" % type(inputs))
  outputs = [_as_indexed_slices(i, optimize=optimize) for i in inputs]
  with_int32_index = [
      o.indices for o in outputs if o.indices.dtype == dtypes.int32
  ]
  if not with_int32_index or len(with_int32_index) == len(outputs):
    return outputs
  casted_outputs = []
  for o in outputs:
    if o.indices.dtype == dtypes.int32:
      casted_outputs.append(
          ops.IndexedSlices(o.values, cast(o.indices, dtypes.int64),
                            o.dense_shape))
    else:
      casted_outputs.append(o)
  return casted_outputs


@tf_export("math.add_n", "add_n")
@dispatch.add_dispatch_support
def add_n(inputs, name=None):
  """Adds all input tensors element-wise.

  `tf.math.add_n` performs the same operation as `tf.math.accumulate_n`, but it
  waits for all of its inputs to be ready before beginning to sum.
  This buffering can result in higher memory consumption when inputs are ready
  at different times, since the minimum temporary storage required is
  proportional to the input size rather than the output size.

  This op does not [broadcast](
  https://docs.scipy.org/doc/numpy-1.13.0/user/basics.broadcasting.html)
  its inputs. If you need broadcasting, use `tf.math.add` (or the `+` operator)
  instead.

  For example:

  >>> a = tf.constant([[3, 5], [4, 8]])
  >>> b = tf.constant([[1, 6], [2, 9]])
  >>> tf.math.add_n([a, b, a])
  <tf.Tensor: shape=(2, 2), dtype=int32, numpy=
  array([[ 7, 16],
         [10, 25]], dtype=int32)>

  Args:
    inputs: A list of `tf.Tensor` or `tf.IndexedSlices` objects, each with the
      same shape and type. `tf.IndexedSlices` objects will be converted into
      dense tensors prior to adding.
    name: A name for the operation (optional).

  Returns:
    A `tf.Tensor` of the same shape and type as the elements of `inputs`.

  Raises:
    ValueError: If `inputs` don't all have same shape and dtype or the shape
    cannot be inferred.
  """
  if not inputs or not isinstance(inputs, (list, tuple)):
    raise ValueError("inputs must be a list of at least one "
                     "Tensor/IndexedSlices with the same dtype and shape")
  inputs = ops.convert_n_to_tensor_or_indexed_slices(inputs)
  if not all(isinstance(x, (ops.Tensor, ops.IndexedSlices)) for x in inputs):
    raise ValueError("inputs must be a list of at least one "
                     "Tensor/IndexedSlices with the same dtype and shape")

  if len(inputs) == 1:
    if isinstance(inputs[0], ops.IndexedSlices):
      values = ops.convert_to_tensor(inputs[0])
    else:
      values = inputs[0]
    if name:
      return array_ops.identity(values, name=name)
    return values
  return gen_math_ops.add_n(inputs, name=name)


@tf_export("math.accumulate_n", v1=["math.accumulate_n", "accumulate_n"])
@deprecation.deprecated_endpoints("accumulate_n")
def accumulate_n(inputs, shape=None, tensor_dtype=None, name=None):
  """Returns the element-wise sum of a list of tensors.

  Optionally, pass `shape` and `tensor_dtype` for shape and type checking,
  otherwise, these are inferred.

  `accumulate_n` performs the same operation as `tf.math.add_n`.

  For example:

  ```python
  a = tf.constant([[1, 2], [3, 4]])
  b = tf.constant([[5, 0], [0, 6]])
  tf.math.accumulate_n([a, b, a])  # [[7, 4], [6, 14]]

  # Explicitly pass shape and type
  tf.math.accumulate_n([a, b, a], shape=[2, 2], tensor_dtype=tf.int32)
                                                                 # [[7,  4],
                                                                 #  [6, 14]]
  ```

  Args:
    inputs: A list of `Tensor` objects, each with same shape and type.
    shape: Expected shape of elements of `inputs` (optional). Also controls the
      output shape of this op, which may affect type inference in other ops. A
      value of `None` means "infer the input shape from the shapes in `inputs`".
    tensor_dtype: Expected data type of `inputs` (optional). A value of `None`
      means "infer the input dtype from `inputs[0]`".
    name: A name for the operation (optional).

  Returns:
    A `Tensor` of same shape and type as the elements of `inputs`.

  Raises:
    ValueError: If `inputs` don't all have same shape and dtype or the shape
    cannot be inferred.
  """

  def _input_error():
    return ValueError("inputs must be a list of at least one Tensor with the "
                      "same dtype and shape")

  if not inputs or not isinstance(inputs, (list, tuple)):
    raise _input_error()
  inputs = ops.convert_n_to_tensor_or_indexed_slices(inputs)
  if not all(isinstance(x, ops.Tensor) for x in inputs):
    raise _input_error()
  if not all(x.dtype == inputs[0].dtype for x in inputs):
    raise _input_error()
  if shape is not None:
    shape = tensor_shape.as_shape(shape)
  else:
    shape = tensor_shape.unknown_shape()
  for input_tensor in inputs:
    if isinstance(input_tensor, ops.Tensor):
      shape = shape.merge_with(input_tensor.get_shape())

  # tensor_dtype is for safety only; operator's output type computed in C++
  if tensor_dtype is not None and tensor_dtype != inputs[0].dtype:
    raise TypeError("tensor_dtype is {}, but input is of type {}".format(
        tensor_dtype, inputs[0].dtype))

  if len(inputs) == 1 and name is None:
    return inputs[0]
  elif len(inputs) == 1 and name is not None:
    return array_ops.identity(inputs[0], name=name)
  return add_n(inputs, name=name)


@ops.RegisterGradient("AccumulateNV2")
def _accumulate_n_grad(op, grad):
  """Same as gradient for AddN. Copies the gradient to all inputs."""
  # Not broadcasting.
  return [grad] * len(op.inputs)


@tf_export("math.sigmoid", "nn.sigmoid", "sigmoid")
def sigmoid(x, name=None):
  """Computes sigmoid of `x` element-wise.

  Specifically, `y = 1 / (1 + exp(-x))`.

  Args:
    x: A Tensor with type `float16`, `float32`, `float64`, `complex64`, or
      `complex128`.
    name: A name for the operation (optional).

  Returns:
    A Tensor with the same type as `x`.

  @compatibility(scipy)
  Equivalent to scipy.special.expit
  @end_compatibility
  """
  with ops.name_scope(name, "Sigmoid", [x]) as name:
    x = ops.convert_to_tensor(x, name="x")
    return gen_math_ops.sigmoid(x, name=name)


@tf_export("math.log_sigmoid", v1=["math.log_sigmoid", "log_sigmoid"])
@dispatch.add_dispatch_support
@deprecation.deprecated_endpoints("log_sigmoid")
def log_sigmoid(x, name=None):
  """Computes log sigmoid of `x` element-wise.

  Specifically, `y = log(1 / (1 + exp(-x)))`.  For numerical stability,
  we use `y = -tf.nn.softplus(-x)`.

  Args:
    x: A Tensor with type `float32` or `float64`.
    name: A name for the operation (optional).

  Returns:
    A Tensor with the same type as `x`.
  """
  with ops.name_scope(name, "LogSigmoid", [x]) as name:
    x = ops.convert_to_tensor(x, name="x")
    return gen_math_ops.neg(gen_nn_ops.softplus(-x), name=name)


@tf_export("math.bincount", v1=[])
def bincount(arr,
             weights=None,
             minlength=None,
             maxlength=None,
             dtype=dtypes.int32,
             name=None):
  """Counts the number of occurrences of each value in an integer array.

  If `minlength` and `maxlength` are not given, returns a vector with length
  `tf.reduce_max(arr) + 1` if `arr` is non-empty, and length 0 otherwise.
  If `weights` are non-None, then index `i` of the output stores the sum of the
  value in `weights` at each index where the corresponding value in `arr` is
  `i`.

  ```python
  values = tf.constant([1,1,2,3,2,4,4,5])
  tf.math.bincount(values) #[0 2 2 1 2 1]
  ```
  Vector length = Maximum element in vector `values` is 5. Adding 1, which is 6
                  will be the vector length.

  Each bin value in the output indicates number of occurrences of the particular
  index. Here, index 1 in output has a value 2. This indicates value 1 occurs
  two times in `values`.

  ```python
  values = tf.constant([1,1,2,3,2,4,4,5])
  weights = tf.constant([1,5,0,1,0,5,4,5])
  tf.math.bincount(values, weights=weights) #[0 6 0 1 9 5]
  ```
  Bin will be incremented by the corresponding weight instead of 1.
  Here, index 1 in output has a value 6. This is the summation of weights
  corresponding to the value in `values`.

  Args:
    arr: An int32 tensor of non-negative values.
    weights: If non-None, must be the same shape as arr. For each value in
      `arr`, the bin will be incremented by the corresponding weight instead of
      1.
    minlength: If given, ensures the output has length at least `minlength`,
      padding with zeros at the end if necessary.
    maxlength: If given, skips values in `arr` that are equal or greater than
      `maxlength`, ensuring that the output has length at most `maxlength`.
    dtype: If `weights` is None, determines the type of the output bins.
    name: A name scope for the associated operations (optional).

  Returns:
    A vector with the same dtype as `weights` or the given `dtype`. The bin
    values.

  Raises:
    `InvalidArgumentError` if negative values are provided as an input.

  """
  name = "bincount" if name is None else name
  with ops.name_scope(name):
    arr = ops.convert_to_tensor(arr, name="arr", dtype=dtypes.int32)
    array_is_nonempty = reduce_prod(array_ops.shape(arr)) > 0
    output_size = cast(array_is_nonempty, dtypes.int32) * (reduce_max(arr) + 1)
    if minlength is not None:
      minlength = ops.convert_to_tensor(
          minlength, name="minlength", dtype=dtypes.int32)
      output_size = gen_math_ops.maximum(minlength, output_size)
    if maxlength is not None:
      maxlength = ops.convert_to_tensor(
          maxlength, name="maxlength", dtype=dtypes.int32)
      output_size = gen_math_ops.minimum(maxlength, output_size)
    if weights is not None:
      weights = ops.convert_to_tensor(weights, name="weights")
      return gen_math_ops.unsorted_segment_sum(weights, arr, output_size)
    weights = constant_op.constant([], dtype)
    return gen_math_ops.bincount(arr, output_size, weights)


@tf_export(v1=["math.bincount", "bincount"])
@deprecation.deprecated_endpoints("bincount")
def bincount_v1(arr,
                weights=None,
                minlength=None,
                maxlength=None,
                dtype=dtypes.int32):
  """Counts the number of occurrences of each value in an integer array.

  If `minlength` and `maxlength` are not given, returns a vector with length
  `tf.reduce_max(arr) + 1` if `arr` is non-empty, and length 0 otherwise.
  If `weights` are non-None, then index `i` of the output stores the sum of the
  value in `weights` at each index where the corresponding value in `arr` is
  `i`.

  Args:
    arr: An int32 tensor of non-negative values.
    weights: If non-None, must be the same shape as arr. For each value in
      `arr`, the bin will be incremented by the corresponding weight instead of
      1.
    minlength: If given, ensures the output has length at least `minlength`,
      padding with zeros at the end if necessary.
    maxlength: If given, skips values in `arr` that are equal or greater than
      `maxlength`, ensuring that the output has length at most `maxlength`.
    dtype: If `weights` is None, determines the type of the output bins.

  Returns:
    A vector with the same dtype as `weights` or the given `dtype`. The bin
    values.
  """
  return bincount(arr, weights, minlength, maxlength, dtype)


@tf_export("math.cumsum", "cumsum")
def cumsum(x, axis=0, exclusive=False, reverse=False, name=None):
  """Compute the cumulative sum of the tensor `x` along `axis`.

  By default, this op performs an inclusive cumsum, which means that the first
  element of the input is identical to the first element of the output:
  For example:

  # tf.cumsum([a, b, c])   # [a, a + b, a + b + c]
  >>> x = tf.constant([2, 4, 6, 8])
  >>> tf.cumsum(x)
  <tf.Tensor: shape=(4,), dtype=int32,
  numpy=array([ 2,  6, 12, 20], dtype=int32)>

  # using varying `axis` values
  >>> y = tf.constant([[2, 4, 6, 8], [1,3,5,7]])
  >>> tf.cumsum(y, axis=0)
  <tf.Tensor: shape=(2, 4), dtype=int32, numpy=
  array([[ 2,  4,  6,  8],
         [ 3,  7, 11, 15]], dtype=int32)>

  >>> tf.cumsum(y, axis=1)
  <tf.Tensor: shape=(2, 4), dtype=int32, numpy=
  array([[ 2,  6, 12, 20],
         [ 1,  4,  9, 16]], dtype=int32)>

  By setting the `exclusive` kwarg to `True`, an exclusive cumsum is performed
  instead:

  # tf.cumsum([a, b, c], exclusive=True)  => [0, a, a + b]
  >>> x = tf.constant([2, 4, 6, 8])
  >>> tf.cumsum(x, exclusive=True)
  <tf.Tensor: shape=(4,), dtype=int32,
  numpy=array([ 0,  2,  6, 12], dtype=int32)>

  By setting the `reverse` kwarg to `True`, the cumsum is performed in the
  opposite direction:

  # tf.cumsum([a, b, c], reverse=True)  # [a + b + c, b + c, c]
  >>> x = tf.constant([2, 4, 6, 8])
  >>> tf.cumsum(x, reverse=True)
  <tf.Tensor: shape=(4,), dtype=int32,
  numpy=array([20, 18, 14,  8], dtype=int32)>

  This is more efficient than using separate `tf.reverse` ops.
  The `reverse` and `exclusive` kwargs can also be combined:

  # tf.cumsum([a, b, c], exclusive=True, reverse=True)  # [b + c, c, 0]
  >>> x = tf.constant([2, 4, 6, 8])
  >>> tf.cumsum(x, exclusive=True, reverse=True)
  <tf.Tensor: shape=(4,), dtype=int32,
  numpy=array([18, 14,  8,  0], dtype=int32)>

  Args:
    x: A `Tensor`. Must be one of the following types: `float32`, `float64`,
      `int64`, `int32`, `uint8`, `uint16`, `int16`, `int8`, `complex64`,
      `complex128`, `qint8`, `quint8`, `qint32`, `half`.
    axis: A `Tensor` of type `int32` (default: 0). Must be in the range
      `[-rank(x), rank(x))`.
    exclusive: If `True`, perform exclusive cumsum.
    reverse: A `bool` (default: False).
    name: A name for the operation (optional).

  Returns:
    A `Tensor`. Has the same type as `x`.
  """
  with ops.name_scope(name, "Cumsum", [x]) as name:
    x = ops.convert_to_tensor(x, name="x")
    return gen_math_ops.cumsum(
        x, axis, exclusive=exclusive, reverse=reverse, name=name)


@tf_export("math.cumprod", v1=["math.cumprod", "cumprod"])
@deprecation.deprecated_endpoints("cumprod")
def cumprod(x, axis=0, exclusive=False, reverse=False, name=None):
  """Compute the cumulative product of the tensor `x` along `axis`.

  By default, this op performs an inclusive cumprod, which means that the
  first element of the input is identical to the first element of the output:

  ```python
  tf.math.cumprod([a, b, c])  # [a, a * b, a * b * c]
  ```

  By setting the `exclusive` kwarg to `True`, an exclusive cumprod is
  performed
  instead:

  ```python
  tf.math.cumprod([a, b, c], exclusive=True)  # [1, a, a * b]
  ```

  By setting the `reverse` kwarg to `True`, the cumprod is performed in the
  opposite direction:

  ```python
  tf.math.cumprod([a, b, c], reverse=True)  # [a * b * c, b * c, c]
  ```

  This is more efficient than using separate `tf.reverse` ops.
  The `reverse` and `exclusive` kwargs can also be combined:

  ```python
  tf.math.cumprod([a, b, c], exclusive=True, reverse=True)  # [b * c, c, 1]
  ```

  Args:
    x: A `Tensor`. Must be one of the following types: `float32`, `float64`,
      `int64`, `int32`, `uint8`, `uint16`, `int16`, `int8`, `complex64`,
      `complex128`, `qint8`, `quint8`, `qint32`, `half`.
    axis: A `Tensor` of type `int32` (default: 0). Must be in the range
      `[-rank(x), rank(x))`.
    exclusive: If `True`, perform exclusive cumprod.
    reverse: A `bool` (default: False).
    name: A name for the operation (optional).

  Returns:
    A `Tensor`. Has the same type as `x`.
  """
  with ops.name_scope(name, "Cumprod", [x]) as name:
    x = ops.convert_to_tensor(x, name="x")
    return gen_math_ops.cumprod(
        x, axis, exclusive=exclusive, reverse=reverse, name=name)


@tf_export("math.cumulative_logsumexp", v1=["math.cumulative_logsumexp"])
def cumulative_logsumexp(x, axis=0, exclusive=False, reverse=False, name=None):
  """Compute the cumulative log-sum-exp of the tensor `x` along `axis`.

  By default, this op performs an inclusive cumulative log-sum-exp, which means
  that the first element of the input is identical to the first element of
  the output.

  This operation is significantly more numerically stable than the equivalent
  tensorflow operation `tf.math.log(tf.math.cumsum(tf.math.exp(x)))`, although
  computes the same result given infinite numerical precision. However, note
  that in some cases, it may be less stable than `tf.math.reduce_logsumexp`
  for a given element, as it applies the "log-sum-exp trick" in a different
  way.

  More precisely, where `tf.math.reduce_logsumexp` uses the following trick:

  ```
  log(sum(exp(x))) == log(sum(exp(x - max(x)))) + max(x)
  ```

  it cannot be directly used here as there is no fast way of applying it
  to each prefix `x[:i]`. Instead, this function implements a prefix
  scan using pairwise log-add-exp, which is a commutative and associative
  (up to floating point precision) operator:

  ```
  log_add_exp(x, y) = log(exp(x) + exp(y))
                    = log(1 + exp(min(x, y) - max(x, y))) + max(x, y)
  ```

  However, reducing using the above operator leads to a different computation
  tree (logs are taken repeatedly instead of only at the end), and the maximum
  is only computed pairwise instead of over the entire prefix. In general, this
  leads to a different and slightly less precise computation.

  Args:
    x: A `Tensor`. Must be one of the following types: `float16`, `float32`,
      `float64`.
    axis: A `Tensor` of type `int32` or `int64` (default: 0). Must be in the
      range `[-rank(x), rank(x))`.
    exclusive: If `True`, perform exclusive cumulative log-sum-exp.
    reverse: If `True`, performs the cumulative log-sum-exp in the reverse
      direction.
    name: A name for the operation (optional).

  Returns:
    A `Tensor`. Has the same shape and type as `x`.
  """
  with ops.name_scope(name, "CumulativeLogsumexp", [x]) as name:
    x = ops.convert_to_tensor(x, name="x")
    return gen_math_ops.cumulative_logsumexp(
        x, axis, exclusive=exclusive, reverse=reverse, name=name)


@tf_export("math.conj", v1=["math.conj", "conj"])
@dispatch.add_dispatch_support
@deprecation.deprecated_endpoints("conj")
def conj(x, name=None):
  r"""Returns the complex conjugate of a complex number.

  Given a tensor `input` of complex numbers, this operation returns a tensor of
  complex numbers that are the complex conjugate of each element in `input`. The
  complex numbers in `input` must be of the form \\(a + bj\\), where *a* is the
  real part and *b* is the imaginary part.

  The complex conjugate returned by this operation is of the form \\(a - bj\\).

  For example:

      # tensor 'input' is [-2.25 + 4.75j, 3.25 + 5.75j]
      tf.math.conj(input) ==> [-2.25 - 4.75j, 3.25 - 5.75j]

  If `x` is real, it is returned unchanged.

  Args:
    x: `Tensor` to conjugate.  Must have numeric or variant type.
    name: A name for the operation (optional).

  Returns:
    A `Tensor` that is the conjugate of `x` (with the same type).

  Raises:
    TypeError: If `x` is not a numeric tensor.
  """
  if isinstance(x, ops.Tensor):
    dt = x.dtype
    if dt.is_floating or dt.is_integer:
      return x
  with ops.name_scope(name, "Conj", [x]) as name:
    x = ops.convert_to_tensor(x, name="x")
    if x.dtype.is_complex or x.dtype == dtypes.variant:
      return gen_math_ops.conj(x, name=name)
    elif x.dtype.is_floating or x.dtype.is_integer:
      return x
    else:
      raise TypeError("Expected numeric or variant tensor, got dtype %r" %
                      x.dtype)


def reduced_shape(input_shape, axes):
  """Helper function for reduction ops.

  Args:
    input_shape: 1-D Tensor, the shape of the Tensor being reduced.
    axes: 1-D Tensor, the reduction axes.

  Returns:
    A 1-D Tensor, the output shape as if keepdims were set to True.
  """
  if context.executing_eagerly():
    input_shape = input_shape.numpy()
    axes = axes.numpy()
    input_shape[axes] = 1
    return input_shape

  # Example:
  # cast needed for SparseTensor reductions
  input_shape = cast(input_shape, dtypes.int32)  # [2, 3, 5, 7]
  axes = cast(axes, dtypes.int32)  # [1, 2]

  input_rank = array_ops.size(input_shape)  # 4
  axes = (axes + input_rank) % input_rank
  axes_shape = array_ops.shape(axes)  # [2]
  return gen_data_flow_ops.dynamic_stitch(  # [2, 1, 1, 7]
      [
          range(input_rank),  # [0, 1, 2, 3]
          axes
      ],  # [1, 2]
      [
          input_shape,  # [2, 3, 5, 7]
          array_ops.fill(axes_shape, 1)
      ])  # [1, 1]


def _unsorted_segment_N(data, segment_ids, num_segments):
  """ Helper function for unsorted_segment_mean/_sqrtN.

  Computes the number
      of segment entries with 0-entries set to 1 to allow division by N.
  """
  num_segments = ops.convert_to_tensor(num_segments)
  # bincount doesn't support negative indices so we use unsorted_segment_sum
  segment_ids_shape = array_ops.shape_internal(segment_ids)
  ones_tensor = array_ops.ones(segment_ids_shape, dtype=data.dtype)
  n = gen_math_ops.unsorted_segment_sum(ones_tensor, segment_ids, num_segments)
  # add dimensions for all non-reduced axes
  broadcastable_shape = array_ops.concat(
      [num_segments[array_ops.newaxis],
       array_ops.ones([array_ops.rank(data)
                       - array_ops.rank(segment_ids)],
                      dtype=num_segments.dtype)],
      axis=0)
  n = array_ops.reshape(n, broadcastable_shape)
  return gen_math_ops.maximum(n, 1)


@tf_export(
    "math.unsorted_segment_mean",
    v1=["math.unsorted_segment_mean", "unsorted_segment_mean"])
@deprecation.deprecated_endpoints("unsorted_segment_mean")
@dispatch.add_dispatch_support
def unsorted_segment_mean(data, segment_ids, num_segments, name=None):
  r"""Computes the mean along segments of a tensor.

  Read [the section on
  segmentation](https://www.tensorflow.org/versions/r2.0/api_docs/python/tf/math#about_segmentation)
  for an explanation of segments.

  This operator is similar to the unsorted segment sum operator found
  [here](../../../api_docs/python/math_ops.md#UnsortedSegmentSum).
  Instead of computing the sum over segments, it computes the mean of all
  entries belonging to a segment such that:

  \\(output_i = 1/N_i \sum_{j...} data[j...]\\) where the sum is over tuples
  `j...` such that `segment_ids[j...] == i` with \\N_i\\ being the number of
  occurrences of id \\i\\.

  If there is no entry for a given segment ID `i`, it outputs 0.

  If the given segment ID `i` is negative, the value is dropped and will not
  be added to the sum of the segment.

  Args:
    data: A `Tensor` with floating point or complex dtype.
    segment_ids: An integer tensor whose shape is a prefix of `data.shape`.
    num_segments: An integer scalar `Tensor`.  The number of distinct segment
      IDs.
    name: A name for the operation (optional).

  Returns:
    A `Tensor`.  Has same shape as data, except for the first `segment_ids.rank`
    dimensions, which are replaced with a single dimension which has size
   `num_segments`.
  """
  with ops.name_scope(name, "UnsortedSegmentMean"):
    data = ops.convert_to_tensor(data)
    segment_ids = ops.convert_to_tensor(segment_ids)
    N = _unsorted_segment_N(data, segment_ids, num_segments)
    summed = gen_math_ops.unsorted_segment_sum(data, segment_ids, num_segments)
    return summed / N


@tf_export(
    "math.unsorted_segment_sqrt_n",
    v1=["math.unsorted_segment_sqrt_n", "unsorted_segment_sqrt_n"])
@deprecation.deprecated_endpoints("unsorted_segment_sqrt_n")
@dispatch.add_dispatch_support
def unsorted_segment_sqrt_n(data, segment_ids, num_segments, name=None):
  r"""Computes the sum along segments of a tensor divided by the sqrt(N).

  Read [the section on
  segmentation](https://www.tensorflow.org/versions/r2.0/api_docs/python/tf/math#about_segmentation)
  for an explanation of segments.

  This operator is similar to the unsorted segment sum operator found
  [here](../../../api_docs/python/math_ops.md#UnsortedSegmentSum).
  Additionally to computing the sum over segments, it divides the results by
  sqrt(N).

  \\(output_i = 1/sqrt(N_i) \sum_{j...} data[j...]\\) where the sum is over
  tuples `j...` such that `segment_ids[j...] == i` with \\N_i\\ being the
  number of occurrences of id \\i\\.

  If there is no entry for a given segment ID `i`, it outputs 0.

  Note that this op only supports floating point and complex dtypes,
  due to tf.sqrt only supporting these types.

  If the given segment ID `i` is negative, the value is dropped and will not
  be added to the sum of the segment.

  Args:
    data: A `Tensor` with floating point or complex dtype.
    segment_ids: An integer tensor whose shape is a prefix of `data.shape`.
    num_segments: An integer scalar `Tensor`.  The number of distinct segment
      IDs.
    name: A name for the operation (optional).

  Returns:
    A `Tensor`.  Has same shape as data, except for the first `segment_ids.rank`
    dimensions, which are replaced with a single dimension which has size
   `num_segments`.
  """
  with ops.name_scope(name, "UnsortedSegmentSqrtN"):
    data = ops.convert_to_tensor(data)
    segment_ids = ops.convert_to_tensor(segment_ids)
    N = _unsorted_segment_N(data, segment_ids, num_segments)
    summed = gen_math_ops.unsorted_segment_sum(data, segment_ids, num_segments)
    return summed / gen_math_ops.sqrt(N)


@tf_export(v1=["sparse.segment_sum", "sparse_segment_sum"])
@deprecation.deprecated_endpoints("sparse_segment_sum")
def sparse_segment_sum(data,
                       indices,
                       segment_ids,
                       name=None,
                       num_segments=None):
  r"""Computes the sum along sparse segments of a tensor.

  Read [the section on
  segmentation](https://www.tensorflow.org/versions/r2.0/api_docs/python/tf/math#about_segmentation)
  for an explanation of segments.

  Like `tf.math.segment_sum`, but `segment_ids` can have rank less than `data`'s
  first dimension, selecting a subset of dimension 0, specified by `indices`.
  `segment_ids` is allowed to have missing ids, in which case the output will
  be zeros at those indices. In those cases `num_segments` is used to determine
  the size of the output.

  For example:

  ```python
  c = tf.constant([[1,2,3,4], [-1,-2,-3,-4], [5,6,7,8]])

  # Select two rows, one segment.
  tf.sparse.segment_sum(c, tf.constant([0, 1]), tf.constant([0, 0]))
  # => [[0 0 0 0]]

  # Select two rows, two segment.
  tf.sparse.segment_sum(c, tf.constant([0, 1]), tf.constant([0, 1]))
  # => [[ 1  2  3  4]
  #     [-1 -2 -3 -4]]

  # With missing segment ids.
  tf.sparse.segment_sum(c, tf.constant([0, 1]), tf.constant([0, 2]),
                        num_segments=4)
  # => [[ 1  2  3  4]
  #     [ 0  0  0  0]
  #     [-1 -2 -3 -4]
  #     [ 0  0  0  0]]

  # Select all rows, two segments.
  tf.sparse.segment_sum(c, tf.constant([0, 1, 2]), tf.constant([0, 0, 1]))
  # => [[0 0 0 0]
  #     [5 6 7 8]]

  # Which is equivalent to:
  tf.math.segment_sum(c, tf.constant([0, 0, 1]))
  ```

  Args:
    data: A `Tensor` with data that will be assembled in the output.
    indices: A 1-D `Tensor` with indices into `data`. Has same rank as
      `segment_ids`.
    segment_ids: A 1-D `Tensor` with indices into the output `Tensor`. Values
      should be sorted and can be repeated.
    name: A name for the operation (optional).
    num_segments: An optional int32 scalar. Indicates the size of the output
      `Tensor`.

  Returns:
    A `tensor` of the shape as data, except for dimension 0 which
    has size `k`, the number of segments specified via `num_segments` or
    inferred for the last element in `segments_ids`.
  """
  if num_segments is not None:
    return gen_math_ops.sparse_segment_sum_with_num_segments(
        data=data,
        indices=indices,
        segment_ids=segment_ids,
        num_segments=num_segments,
        name=name)
  else:
    return gen_math_ops.sparse_segment_sum(
        data=data, indices=indices, segment_ids=segment_ids, name=name)


@tf_export("sparse.segment_sum", v1=[])
def sparse_segment_sum_v2(data,
                          indices,
                          segment_ids,
                          num_segments=None,
                          name=None):
  r"""Computes the sum along sparse segments of a tensor.

  Read [the section on
  segmentation](https://www.tensorflow.org/versions/r2.0/api_docs/python/tf/math#about_segmentation)
  for an explanation of segments.

  Like `tf.math.segment_sum`, but `segment_ids` can have rank less than `data`'s
  first dimension, selecting a subset of dimension 0, specified by `indices`.
  `segment_ids` is allowed to have missing ids, in which case the output will
  be zeros at those indices. In those cases `num_segments` is used to determine
  the size of the output.

  For example:

  ```python
  c = tf.constant([[1,2,3,4], [-1,-2,-3,-4], [5,6,7,8]])

  # Select two rows, one segment.
  tf.sparse.segment_sum(c, tf.constant([0, 1]), tf.constant([0, 0]))
  # => [[0 0 0 0]]

  # Select two rows, two segment.
  tf.sparse.segment_sum(c, tf.constant([0, 1]), tf.constant([0, 1]))
  # => [[ 1  2  3  4]
  #     [-1 -2 -3 -4]]

  # With missing segment ids.
  tf.sparse.segment_sum(c, tf.constant([0, 1]), tf.constant([0, 2]),
                        num_segments=4)
  # => [[ 1  2  3  4]
  #     [ 0  0  0  0]
  #     [-1 -2 -3 -4]
  #     [ 0  0  0  0]]

  # Select all rows, two segments.
  tf.sparse.segment_sum(c, tf.constant([0, 1, 2]), tf.constant([0, 0, 1]))
  # => [[0 0 0 0]
  #     [5 6 7 8]]

  # Which is equivalent to:
  tf.math.segment_sum(c, tf.constant([0, 0, 1]))
  ```

  Args:
    data: A `Tensor` with data that will be assembled in the output.
    indices: A 1-D `Tensor` with indices into `data`. Has same rank as
      `segment_ids`.
    segment_ids: A 1-D `Tensor` with indices into the output `Tensor`. Values
      should be sorted and can be repeated.
    num_segments: An optional int32 scalar. Indicates the size of the output
      `Tensor`.
    name: A name for the operation (optional).

  Returns:
    A `tensor` of the shape as data, except for dimension 0 which
    has size `k`, the number of segments specified via `num_segments` or
    inferred for the last element in `segments_ids`.
  """
  return sparse_segment_sum(
      data, indices, segment_ids, name=name, num_segments=num_segments)


@tf_export(v1=["sparse.segment_mean", "sparse_segment_mean"])
@deprecation.deprecated_endpoints("sparse_segment_mean")
def sparse_segment_mean(data,
                        indices,
                        segment_ids,
                        name=None,
                        num_segments=None):
  r"""Computes the mean along sparse segments of a tensor.

  Read [the section on
  segmentation](https://www.tensorflow.org/versions/r2.0/api_docs/python/tf/math#about_segmentation)
  for an explanation of segments.

  Like `tf.math.segment_mean`, but `segment_ids` can have rank less than
  `data`'s first dimension, selecting a subset of dimension 0, specified by
  `indices`.
  `segment_ids` is allowed to have missing ids, in which case the output will
  be zeros at those indices. In those cases `num_segments` is used to determine
  the size of the output.

  Args:
    data: A `Tensor` with data that will be assembled in the output.
    indices: A 1-D `Tensor` with indices into `data`. Has same rank as
      `segment_ids`.
    segment_ids: A 1-D `Tensor` with indices into the output `Tensor`. Values
      should be sorted and can be repeated.
    name: A name for the operation (optional).
    num_segments: An optional int32 scalar. Indicates the size of the output
      `Tensor`.

  Returns:
    A `tensor` of the shape as data, except for dimension 0 which
    has size `k`, the number of segments specified via `num_segments` or
    inferred for the last element in `segments_ids`.
  """
  if num_segments is not None:
    return gen_math_ops.sparse_segment_mean_with_num_segments(
        data=data,
        indices=indices,
        segment_ids=segment_ids,
        num_segments=num_segments,
        name=name)
  else:
    return gen_math_ops.sparse_segment_mean(
        data=data, indices=indices, segment_ids=segment_ids, name=name)


@tf_export("sparse.segment_mean", v1=[])
def sparse_segment_mean_v2(data,
                           indices,
                           segment_ids,
                           num_segments=None,
                           name=None):
  r"""Computes the mean along sparse segments of a tensor.

  Read [the section on
  segmentation](https://www.tensorflow.org/versions/r2.0/api_docs/python/tf/math#about_segmentation)
  for an explanation of segments.

  Like `tf.math.segment_mean`, but `segment_ids` can have rank less than
  `data`'s first dimension, selecting a subset of dimension 0, specified by
  `indices`.
  `segment_ids` is allowed to have missing ids, in which case the output will
  be zeros at those indices. In those cases `num_segments` is used to determine
  the size of the output.

  Args:
    data: A `Tensor` with data that will be assembled in the output.
    indices: A 1-D `Tensor` with indices into `data`. Has same rank as
      `segment_ids`.
    segment_ids: A 1-D `Tensor` with indices into the output `Tensor`. Values
      should be sorted and can be repeated.
    num_segments: An optional int32 scalar. Indicates the size of the output
      `Tensor`.
    name: A name for the operation (optional).

  Returns:
    A `tensor` of the shape as data, except for dimension 0 which
    has size `k`, the number of segments specified via `num_segments` or
    inferred for the last element in `segments_ids`.
  """
  return sparse_segment_mean(
      data, indices, segment_ids, name=name, num_segments=num_segments)


@tf_export(v1=["sparse.segment_sqrt_n", "sparse_segment_sqrt_n"])
@deprecation.deprecated_endpoints("sparse_segment_sqrt_n")
def sparse_segment_sqrt_n(data,
                          indices,
                          segment_ids,
                          name=None,
                          num_segments=None):
  r"""Computes the sum along sparse segments of a tensor divided by the sqrt(N).

  `N` is the size of the segment being reduced.

  Args:
    data: A `Tensor` with data that will be assembled in the output.
    indices: A 1-D `Tensor` with indices into `data`. Has same rank as
      `segment_ids`.
    segment_ids: A 1-D `Tensor` with indices into the output `Tensor`. Values
      should be sorted and can be repeated.
    name: A name for the operation (optional).
    num_segments: An optional int32 scalar. Indicates the size of the output
      `Tensor`.

  Returns:
    A `tensor` of the shape as data, except for dimension 0 which
    has size `k`, the number of segments specified via `num_segments` or
    inferred for the last element in `segments_ids`.
  """
  if num_segments is not None:
    return gen_math_ops.sparse_segment_sqrt_n_with_num_segments(
        data=data,
        indices=indices,
        segment_ids=segment_ids,
        num_segments=num_segments,
        name=name)
  else:
    return gen_math_ops.sparse_segment_sqrt_n(
        data=data, indices=indices, segment_ids=segment_ids, name=name)


@tf_export("sparse.segment_sqrt_n", v1=[])
def sparse_segment_sqrt_n_v2(data,
                             indices,
                             segment_ids,
                             num_segments=None,
                             name=None):
  r"""Computes the sum along sparse segments of a tensor divided by the sqrt(N).

  Read [the section on
  segmentation](https://www.tensorflow.org/versions/r2.0/api_docs/python/tf/math#about_segmentation)
  for an explanation of segments.

  Like `tf.sparse.segment_mean`, but instead of dividing by the size of the
  segment, `N`, divide by `sqrt(N)` instead.

  Args:
    data: A `Tensor` with data that will be assembled in the output.
    indices: A 1-D `Tensor` with indices into `data`. Has same rank as
      `segment_ids`.
    segment_ids: A 1-D `Tensor` with indices into the output `Tensor`. Values
      should be sorted and can be repeated.
    num_segments: An optional int32 scalar. Indicates the size of the output
      `Tensor`.
    name: A name for the operation (optional).

  Returns:
    A `tensor` of the shape as data, except for dimension 0 which
    has size `k`, the number of segments specified via `num_segments` or
    inferred for the last element in `segments_ids`.
  """
  return sparse_segment_sqrt_n(
      data, indices, segment_ids, name=name, num_segments=num_segments)


@tf_export("tensordot", "linalg.tensordot")
def tensordot(a, b, axes, name=None):
  r"""Tensor contraction of a and b along specified axes and outer product.

  Tensordot (also known as tensor contraction) sums the product of elements
  from `a` and `b` over the indices specified by `a_axes` and `b_axes`.
  The lists `a_axes` and `b_axes` specify those pairs of axes along which to
  contract the tensors. The axis `a_axes[i]` of `a` must have the same dimension
  as axis `b_axes[i]` of `b` for all `i` in `range(0, len(a_axes))`. The lists
  `a_axes` and `b_axes` must have identical length and consist of unique
  integers that specify valid axes for each of the tensors. Additionally
  outer product is supported by passing `axes=0`.

  This operation corresponds to `numpy.tensordot(a, b, axes)`.

  Example 1: When `a` and `b` are matrices (order 2), the case `axes = 1`
  is equivalent to matrix multiplication.

  Example 2: When `a` and `b` are matrices (order 2), the case
  `axes = [[1], [0]]` is equivalent to matrix multiplication.

  Example 3: When `a` and `b` are matrices (order 2), the case `axes=0` gives
  the outer product, a tensor of order 4.

  Example 4: Suppose that \\(a_{ijk}\\) and \\(b_{lmn}\\) represent two
  tensors of order 3. Then, `contract(a, b, [[0], [2]])` is the order 4 tensor
  \\(c_{jklm}\\) whose entry
  corresponding to the indices \\((j,k,l,m)\\) is given by:

  \\( c_{jklm} = \sum_i a_{ijk} b_{lmi} \\).

  In general, `order(c) = order(a) + order(b) - 2*len(axes[0])`.

  Args:
    a: `Tensor` of type `float32` or `float64`.
    b: `Tensor` with the same type as `a`.
    axes: Either a scalar `N`, or a list or an `int32` `Tensor` of shape [2, k].
      If axes is a scalar, sum over the last N axes of a and the first N axes of
      b in order. If axes is a list or `Tensor` the first and second row contain
      the set of unique integers specifying axes along which the contraction is
      computed, for `a` and `b`, respectively. The number of axes for `a` and
      `b` must be equal. If `axes=0`, computes the outer product between `a` and
      `b`.
    name: A name for the operation (optional).

  Returns:
    A `Tensor` with the same type as `a`.

  Raises:
    ValueError: If the shapes of `a`, `b`, and `axes` are incompatible.
    IndexError: If the values in axes exceed the rank of the corresponding
      tensor.
  """

  def _tensordot_reshape(a, axes, flipped=False):
    """Helper method to perform transpose and reshape for contraction op.

    This method is helpful in reducing `math_ops.tensordot` to `math_ops.matmul`
    using `array_ops.transpose` and `array_ops.reshape`. The method takes a
    tensor and performs the correct transpose and reshape operation for a given
    set of indices. It returns the reshaped tensor as well as a list of indices
    necessary to reshape the tensor again after matrix multiplication.

    Args:
      a: `Tensor`.
      axes: List or `int32` `Tensor` of unique indices specifying valid axes of
        `a`.
      flipped: An optional `bool`. Defaults to `False`. If `True`, the method
        assumes that `a` is the second argument in the contraction operation.

    Returns:
      A tuple `(reshaped_a, free_dims, free_dims_static)` where `reshaped_a` is
      the tensor `a` reshaped to allow contraction via `matmul`, `free_dims` is
      either a list of integers or an `int32` `Tensor`, depending on whether
      the shape of a is fully specified, and free_dims_static is either a list
      of integers and None values, or None, representing the inferred
      static shape of the free dimensions
    """
    if a.get_shape().is_fully_defined() and isinstance(axes, (list, tuple)):
      shape_a = a.get_shape().as_list()
      axes = [i if i >= 0 else i + len(shape_a) for i in axes]
      free = [i for i in xrange(len(shape_a)) if i not in axes]
      free_dims = [shape_a[i] for i in free]
      prod_free = int(np.prod([shape_a[i] for i in free]))
      prod_axes = int(np.prod([shape_a[i] for i in axes]))
      perm = list(axes) + free if flipped else free + list(axes)
      new_shape = [prod_axes, prod_free] if flipped else [prod_free, prod_axes]
      reshaped_a = array_ops.reshape(array_ops.transpose(a, perm), new_shape)
      return reshaped_a, free_dims, free_dims
    else:
      if a.get_shape().ndims is not None and isinstance(axes, (list, tuple)):
        shape_a = a.get_shape().as_list()
        axes = [i if i >= 0 else i + len(shape_a) for i in axes]
        free = [i for i in xrange(len(shape_a)) if i not in axes]
        axes_dims = [shape_a[i] for i in axes]
        free_dims = [shape_a[i] for i in free]
        free_dims_static = free_dims
        axes = ops.convert_to_tensor(axes, dtype=dtypes.int32, name="axes")
        free = ops.convert_to_tensor(free, dtype=dtypes.int32, name="free")
        shape_a = array_ops.shape(a)
      else:
        free_dims_static = None
        shape_a = array_ops.shape(a)
        rank_a = array_ops.rank(a)
        axes = ops.convert_to_tensor(axes, dtype=dtypes.int32, name="axes")
        axes = array_ops.where(axes >= 0, axes, axes + rank_a)
        free, _ = array_ops.setdiff1d(range(rank_a), axes)
      free_dims = array_ops.gather(shape_a, free)
      axes_dims = array_ops.gather(shape_a, axes)
      prod_free_dims = reduce_prod(free_dims)
      prod_axes_dims = reduce_prod(axes_dims)
      if flipped:
        perm = array_ops.concat([axes, free], 0)
        new_shape = array_ops.stack([prod_axes_dims, prod_free_dims])
      else:
        perm = array_ops.concat([free, axes], 0)
        new_shape = array_ops.stack([prod_free_dims, prod_axes_dims])
      reshaped_a = array_ops.reshape(array_ops.transpose(a, perm), new_shape)
      return reshaped_a, free_dims, free_dims_static

  def _tensordot_axes(a, axes):
    """Generates two sets of contraction axes for the two tensor arguments."""
    a_shape = a.get_shape()
    if isinstance(axes, compat.integral_types):
      if axes < 0:
        raise ValueError("'axes' must be at least 0.")
      if a_shape.ndims is not None:
        if axes > a_shape.ndims:
          raise ValueError("'axes' must not be larger than the number of "
                           "dimensions of tensor %s." % a)
        return (list(xrange(a_shape.ndims - axes,
                            a_shape.ndims)), list(xrange(axes)))
      else:
        rank = array_ops.rank(a)
        return (range(rank - axes, rank,
                      dtype=dtypes.int32), range(axes, dtype=dtypes.int32))
    elif isinstance(axes, (list, tuple)):
      if len(axes) != 2:
        raise ValueError("'axes' must be an integer or have length 2.")
      a_axes = axes[0]
      b_axes = axes[1]
      if isinstance(a_axes, compat.integral_types) and \
          isinstance(b_axes, compat.integral_types):
        a_axes = [a_axes]
        b_axes = [b_axes]
      if len(a_axes) != len(b_axes):
        raise ValueError(
            "Different number of contraction axes 'a' and 'b', %s != %s." %
            (len(a_axes), len(b_axes)))
      return a_axes, b_axes
    else:
      axes = ops.convert_to_tensor(axes, name="axes", dtype=dtypes.int32)
      return axes[0], axes[1]

  with ops.name_scope(name, "Tensordot", [a, b, axes]) as name:
    a = ops.convert_to_tensor(a, name="a")
    b = ops.convert_to_tensor(b, name="b")
    a_axes, b_axes = _tensordot_axes(a, axes)
    a_reshape, a_free_dims, a_free_dims_static = _tensordot_reshape(a, a_axes)
    b_reshape, b_free_dims, b_free_dims_static = _tensordot_reshape(
        b, b_axes, True)
    ab_matmul = matmul(a_reshape, b_reshape)
    if isinstance(a_free_dims, list) and isinstance(b_free_dims, list):
      return array_ops.reshape(ab_matmul, a_free_dims + b_free_dims, name=name)
    else:
      a_free_dims = ops.convert_to_tensor(a_free_dims, dtype=dtypes.int32)
      b_free_dims = ops.convert_to_tensor(b_free_dims, dtype=dtypes.int32)
      product = array_ops.reshape(
          ab_matmul, array_ops.concat([a_free_dims, b_free_dims], 0), name=name)
      if a_free_dims_static is not None and b_free_dims_static is not None:
        product.set_shape(a_free_dims_static + b_free_dims_static)
      return product


@tf_export("math.polyval")
def polyval(coeffs, x, name=None):
  r"""Computes the elementwise value of a polynomial.

  If `x` is a tensor and `coeffs` is a list n + 1 tensors,
  this function returns the value of the n-th order polynomial

     p(x) = coeffs[n-1] + coeffs[n-2] * x + ...  + coeffs[0] * x**(n-1)

  evaluated using Horner's method, i.e.

     p(x) = coeffs[n-1] + x * (coeffs[n-2] + ... + x * (coeffs[1] +
            x * coeffs[0]))

  Args:
    coeffs: A list of `Tensor` representing the coefficients of the polynomial.
    x: A `Tensor` representing the variable of the polynomial.
    name: A name for the operation (optional).

  Returns:
    A `tensor` of the shape as the expression p(x) with usual broadcasting
    rules for element-wise addition and multiplication applied.

  @compatibility(numpy)
  Equivalent to numpy.polyval.
  @end_compatibility
  """

  with ops.name_scope(name, "polyval", nest.flatten(coeffs) + [x]) as name:
    x = ops.convert_to_tensor(x, name="x")
    if len(coeffs) < 1:
      return array_ops.zeros_like(x, name=name)
    coeffs = [
        ops.convert_to_tensor(coeff, name=("coeff_%d" % index))
        for index, coeff in enumerate(coeffs)
    ]
    p = coeffs[0]
    for c in coeffs[1:]:
      p = c + p * x
    return p


@tf_export("math.reciprocal_no_nan")
def reciprocal_no_nan(x, name=None):
  """Performs a safe reciprocal operation, element wise.

  If a particular element is zero, the reciprocal for that element is
  also set to zero.

  For example:
  ```python
  x = tf.constant([2.0, 0.5, 0, 1], dtype=tf.float32)
  tf.math.reciprocal_no_nan(x)  # [ 0.5, 2, 0.0, 1.0 ]
  ```

  Args:
    x: A `Tensor` of type `float16`, `float32`, `float64` `complex64` or
      `complex128`.
    name: A name for the operation (optional).

  Returns:
    A `Tensor` of same shape and type as `x`.

  Raises:
    TypeError: x must be of a valid dtype.

  """

  with ops.name_scope(name, "reciprocal_no_nan", [x]) as scope:
    x = ops.convert_to_tensor(x, name="x")
    one = constant_op.constant(1, dtype=x.dtype.base_dtype, name="one")
    return gen_math_ops.div_no_nan(one, x, name=scope)


@tf_export("math.erfinv")
@dispatch.add_dispatch_support
def erfinv(x, name=None):
  """Compute inverse error function.

  Given `x`, compute the inverse error function of `x`. This function
  is the inverse of `tf.math.erf`.

  Args:
    x: `Tensor` with type `float` or `double`.
    name: A name for the operation (optional).
  Returns:
    Inverse error function of `x`.
  """
  with ops.name_scope(name, "erfinv", [x]):
    return gen_math_ops.erfinv(x)


@tf_export("math.ndtri")
@dispatch.add_dispatch_support
def ndtri(x, name=None):
  """Compute quantile of Standard Normal.

  Args:
    x: `Tensor` with type `float` or `double`.
    name: A name for the operation (optional).
  Returns:
    Inverse error function of `x`.
  """
  with ops.name_scope(name, "ndtri", [x]):
    return gen_math_ops.ndtri(x)


@tf_export("math.ceil", v1=["math.ceil", "ceil"])
@deprecation.deprecated_endpoints("ceil")
@dispatch.add_dispatch_support
def ceil(x, name=None):
  """Return the ceiling of the input, element-wise.

  For example:

  >>> tf.math.ceil([-1.7, -1.5, -0.2, 0.2, 1.5, 1.7, 2.0])
  <tf.Tensor: shape=(7,), dtype=float32,
  numpy=array([-1., -1., -0.,  1.,  2.,  2.,  2.], dtype=float32)>

  Args:
    x: A `tf.Tensor`. Must be one of the following types: `bfloat16`, `half`,
      `float32`, `float64`. `int32`
    name: A name for the operation (optional).

  Returns:
    A `tf.Tensor`. Has the same type as `x`.

  @compatibility(numpy)
  Equivalent to np.ceil
  @end_compatibility
  """
  return gen_math_ops.ceil(x, name)


@tf_export("math.sqrt", "sqrt")
@dispatch.add_dispatch_support
def sqrt(x, name=None):  # pylint: disable=redefined-builtin
  r"""Computes element-wise square root of the input tensor.

  Note: This operation does not support integer types.

  >>> x = tf.constant([[4.0], [16.0]])
  >>> tf.sqrt(x)
  <tf.Tensor: shape=(2, 1), dtype=float32, numpy=
    array([[2.],
           [4.]], dtype=float32)>
  >>> y = tf.constant([[-4.0], [16.0]])
  >>> tf.sqrt(y)
  <tf.Tensor: shape=(2, 1), dtype=float32, numpy=
    array([[nan],
           [ 4.]], dtype=float32)>
  >>> z = tf.constant([[-1.0], [16.0]], dtype=tf.complex128)
  >>> tf.sqrt(z)
  <tf.Tensor: shape=(2, 1), dtype=complex128, numpy=
    array([[0.0+1.j],
           [4.0+0.j]])>

  Note: In order to support complex complex, please provide an input tensor
  of `complex64` or `complex128`.

  Args:
    x: A `tf.Tensor` of type `bfloat16`, `half`, `float32`, `float64`,
      `complex64`, `complex128`
    name: A name for the operation (optional).

  Returns:
    A `tf.Tensor` of same size, type and sparsity as `x`.
  """
  return gen_math_ops.sqrt(x, name)


# pylint: disable=g-docstring-has-escape
@tf_export("math.exp", "exp")
@dispatch.add_dispatch_support
def exp(x, name=None):
  """Computes exponential of x element-wise.  \\(y = e^x\\).

  This function computes the exponential of the input tensor element-wise.
  i.e. `math.exp(x)` or \\(e^x\\), where `x` is the input tensor.
  \\(e\\) denotes Euler's number and is approximately equal to 2.718281.
  Output is positive for any real input.

  >>> x = tf.constant(2.0)
  >>> tf.math.exp(x)
  <tf.Tensor: shape=(), dtype=float32, numpy=7.389056>

  >>> x = tf.constant([2.0, 8.0])
  >>> tf.math.exp(x)
  <tf.Tensor: shape=(2,), dtype=float32,
  numpy=array([   7.389056, 2980.958   ], dtype=float32)>

  For complex numbers, the exponential value is calculated as
  \\(e^{x+iy}={e^x}{e^{iy}}={e^x}{\\cos(y)+i\\sin(y)}\\)

  For `1+1j` the value would be computed as:
  \\(e^1{\\cos(1)+i\\sin(1)} = 2.7182817 \\times (0.5403023+0.84147096j)\\)

  >>> x = tf.constant(1 + 1j)
  >>> tf.math.exp(x)
  <tf.Tensor: shape=(), dtype=complex128,
  numpy=(1.4686939399158851+2.2873552871788423j)>

  Args:
    x: A `tf.Tensor`. Must be one of the following types: `bfloat16`, `half`,
      `float32`, `float64`, `complex64`, `complex128`.
    name: A name for the operation (optional).

  Returns:
    A `tf.Tensor`. Has the same type as `x`.

  @compatibility(numpy)
  Equivalent to np.exp
  @end_compatibility
  """
  return gen_math_ops.exp(x, name)


# pylint: enable=g-docstring-has-escape
