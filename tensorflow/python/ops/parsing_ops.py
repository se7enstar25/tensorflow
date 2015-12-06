# Copyright 2015 Google Inc. All Rights Reserved.
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

"""Parsing Ops."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import re

from tensorflow.python.framework import ops
from tensorflow.python.framework import tensor_shape
from tensorflow.python.framework import tensor_util
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import common_shapes
from tensorflow.python.ops import constant_op
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.ops import gen_parsing_ops
from tensorflow.python.ops import logging_ops
from tensorflow.python.ops import math_ops
# pylint: disable=wildcard-import,undefined-variable
from tensorflow.python.ops.gen_parsing_ops import *


ops.NoGradient("DecodeRaw")
ops.NoGradient("StringToNumber")


# pylint: disable=protected-access
def parse_example(serialized,
                  names=None,
                  sparse_keys=None,
                  sparse_types=None,
                  dense_keys=None,
                  dense_types=None,
                  dense_defaults=None,
                  dense_shapes=None,
                  name="ParseExample"):
  """Parses `Example` protos.

  Parses a number of serialized [`Example`]
  (https://tensorflow.googlesource.com/tensorflow/+/master/tensorflow/core/example/example.proto)
  protos given in `serialized`.

  `names` may contain descriptive names for the corresponding serialized protos.
  These may be useful for debugging purposes, but they have no effect on the
  output. If not `None`, `names` must be the same length as `serialized`.

  This op parses serialized examples into a dictionary mapping keys to `Tensor`
  and `SparseTensor` objects respectively, depending on whether the keys appear
  in `sparse_keys` or `dense_keys`.

  The key `dense_keys[j]` is mapped to a `Tensor` of type `dense_types[j]` and
  of shape `(serialized.size(),) + dense_shapes[j]`.

  `dense_defaults` provides defaults for values referenced using `dense_keys`.
  If a key is not present in this dictionary, the corresponding dense `Feature`
  is required in all elements of `serialized`.

  `dense_shapes[j]` provides the shape of each `Feature` entry referenced by
  `dense_keys[j]`. The number of elements in the `Feature` corresponding to
  `dense_key[j]` must always have `np.prod(dense_shapes[j])` entries. The
  returned `Tensor` for `dense_key[j]` has shape `[N] + dense_shape[j]`, where
  `N` is the number of `Example`s in `serialized`.

  The key `sparse_keys[j]` is mapped to a `SparseTensor` of type
  `sparse_types[j]`. The `SparseTensor` represents a ragged matrix.
  Its indices are `[batch, index]` where `batch` is the batch entry the value
  is from, and `index` is the value's index in the list of values associated
  with that feature and example.

  Examples:

  For example, if one expects a `tf.float32` sparse feature `ft` and three
  serialized `Example`s are provided:

  ```
  serialized = [
    features
      { feature { key: "ft" value { float_list { value: [1.0, 2.0] } } } },
    features
      { feature []},
    features
      { feature { key: "ft" value { float_list { value: [3.0] } } }
  ]
  ```

  then the output will look like:

  ```
  {"ft": SparseTensor(indices=[[0, 0], [0, 1], [2, 0]],
                      values=[1.0, 2.0, 3.0],
                      shape=(3, 2)) }
  ```

  Given two `Example` input protos in `serialized`:

  ```
  [
    features {
      feature { key: "kw" value { bytes_list { value: [ "knit", "big" ] } } }
      feature { key: "gps" value { float_list { value: [] } } }
    },
    features {
      feature { key: "kw" value { bytes_list { value: [ "emmy" ] } } }
      feature { key: "dank" value { int64_list { value: [ 42 ] } } }
      feature { key: "gps" value { } }
    }
  ]
  ```

  And arguments

  ```
    names: ["input0", "input1"],
    sparse_keys: ["kw", "dank", "gps"]
    sparse_types: [DT_STRING, DT_INT64, DT_FLOAT]
  ```

  Then the output is a dictionary:

  ```python
  {
    "kw": SparseTensor(
        indices=[[0, 0], [0, 1], [1, 0]],
        values=["knit", "big", "emmy"]
        shape=[2, 2]),
    "dank": SparseTensor(
        indices=[[1, 0]],
        values=[42],
        shape=[2, 1]),
    "gps": SparseTensor(
        indices=[],
        values=[],
        shape=[2, 0]),
  }
  ```

  For dense results in two serialized `Example`s:

  ```
  [
    features {
      feature { key: "age" value { int64_list { value: [ 0 ] } } }
      feature { key: "gender" value { bytes_list { value: [ "f" ] } } }
     },
     features {
      feature { key: "age" value { int64_list { value: [] } } }
      feature { key: "gender" value { bytes_list { value: [ "f" ] } } }
    }
  ]
  ```

  We can use arguments:

  ```
  names: ["input0", "input1"],
  dense_keys: np.array(["age", "gender"]),
  dense_types: [tf.int64, tf.string],
  dense_defaults: {
    "age": -1  # "age" defaults to -1 if missing
               # "gender" has no specified default so it's required
  }
  dense_shapes: [(1,), (1,)],  # age, gender, label, weight
  ```

  And the expected output is:

  ```python
  {
    "age": [[0], [-1]],
    "gender": [["f"], ["f"]],
  }
  ```

  Args:
    serialized: A vector (1-D Tensor) of strings, a batch of binary
      serialized `Example` protos.
    names: A vector (1-D Tensor) of strings (optional), the names of
      the serialized protos.
    sparse_keys: A list of string keys in the examples' features.
      The results for these keys will be returned as `SparseTensor` objects.
    sparse_types: A list of `DTypes` of the same length as `sparse_keys`.
      Only `tf.float32` (`FloatList`), `tf.int64` (`Int64List`),
      and `tf.string` (`BytesList`) are supported.
    dense_keys: A list of string keys in the examples' features.
      The results for these keys will be returned as `Tensor`s
    dense_types: A list of DTypes of the same length as `dense_keys`.
      Only `tf.float32` (`FloatList`), `tf.int64` (`Int64List`),
      and `tf.string` (`BytesList`) are supported.
    dense_defaults: A dict mapping string keys to `Tensor`s.
      The keys of the dict must match the dense_keys of the feature.
    dense_shapes: A list of tuples with the same length as `dense_keys`.
      The shape of the data for each dense feature referenced by `dense_keys`.
      Required for any input tensors identified by `dense_keys` whose shapes are
      anything other than `[]` or `[1]`.
    name: A name for this operation (optional).

  Returns:
    A `dict` mapping keys to `Tensor`s and `SparseTensor`s.

  Raises:
    ValueError: If sparse and dense key sets intersect, or input lengths do not
      match up.
  """
  with ops.op_scope([serialized, names], name, "parse_example"):
    names = [] if names is None else names
    dense_defaults = {} if dense_defaults is None else dense_defaults
    sparse_keys = [] if sparse_keys is None else sparse_keys
    sparse_types = [] if sparse_types is None else sparse_types
    dense_keys = [] if dense_keys is None else dense_keys
    dense_types = [] if dense_types is None else dense_types
    dense_shapes = (
        [[]] * len(dense_keys) if dense_shapes is None else dense_shapes)

    num_dense = len(dense_keys)
    num_sparse = len(sparse_keys)

    if len(dense_shapes) != num_dense:
      raise ValueError("len(dense_shapes) != len(dense_keys): %d vs. %d"
                       % (len(dense_shapes), num_dense))
    if len(dense_types) != num_dense:
      raise ValueError("len(dense_types) != len(num_dense): %d vs. %d"
                       % (len(dense_types), num_dense))
    if len(sparse_types) != num_sparse:
      raise ValueError("len(sparse_types) != len(sparse_keys): %d vs. %d"
                       % (len(sparse_types), num_sparse))
    if num_dense + num_sparse == 0:
      raise ValueError("Must provide at least one sparse key or dense key")
    if not set(dense_keys).isdisjoint(set(sparse_keys)):
      raise ValueError(
          "Dense and sparse keys must not intersect; intersection: %s" %
          set(dense_keys).intersection(set(sparse_keys)))

    dense_defaults_vec = []
    for i, key in enumerate(dense_keys):
      default_value = dense_defaults.get(key)
      if default_value is None:
        default_value = constant_op.constant([], dtype=dense_types[i])
      elif not isinstance(default_value, ops.Tensor):
        key_name = "key_" + re.sub("[^A-Za-z0-9_.\\-/]", "_", key)
        default_value = ops.convert_to_tensor(
            default_value, dtype=dense_types[i], name=key_name)
        default_value = array_ops.reshape(default_value, dense_shapes[i])

      dense_defaults_vec.append(default_value)

    dense_shapes = [tensor_util.MakeTensorShapeProto(shape)
                    if isinstance(shape, (list, tuple)) else shape
                    for shape in dense_shapes]

    outputs = gen_parsing_ops._parse_example(
        serialized=serialized,
        names=names,
        dense_defaults=dense_defaults_vec,
        sparse_keys=sparse_keys,
        sparse_types=sparse_types,
        dense_keys=dense_keys,
        dense_shapes=dense_shapes,
        name=name)

    (sparse_indices, sparse_values, sparse_shapes, dense_values) = outputs

    sparse_tensors = [ops.SparseTensor(ix, val, shape) for (ix, val, shape)
                      in zip(sparse_indices, sparse_values, sparse_shapes)]

    return dict(
        zip(sparse_keys + dense_keys, sparse_tensors + dense_values))


def parse_single_example(serialized,  # pylint: disable=invalid-name
                         names=None,
                         sparse_keys=None,
                         sparse_types=None,
                         dense_keys=None,
                         dense_types=None,
                         dense_defaults=None,
                         dense_shapes=None,
                         name="ParseSingleExample"):
  """Parses a single `Example` proto.

  Similar to `parse_example`, except:

  For dense tensors, the returned `Tensor` is identical to the output of
  `parse_example`, except there is no batch dimension, the output shape is the
  same as the shape given in `dense_shape`.

  For `SparseTensor`s, the first (batch) column of the indices matrix is removed
  (the indices matrix is a column vector), the values vector is unchanged, and
  the first (`batch_size`) entry of the shape vector is removed (it is now a
  single element vector).

  See also `parse_example`.

  Args:
    serialized: A scalar string Tensor, a single serialized Example.
      See `parse_example` documentation for more details.
    names: (Optional) A scalar string Tensor, the associated name.
      See `parse_example` documentation for more details.
    sparse_keys: See `parse_example` documentation for more details.
    sparse_types: See `parse_example` documentation for more details.
    dense_keys: See `parse_example` documentation for more details.
    dense_types: See `parse_example` documentation for more details.
    dense_defaults: See `parse_example` documentation for more details.
    dense_shapes: See `parse_example` documentation for more details.
    name: A name for this operation (optional).

  Returns:
    A dictionary mapping keys to Tensors and SparseTensors.

  Raises:
    ValueError: if "scalar" or "names" have known shapes, and are not scalars.
  """
  with ops.op_scope([serialized, names], name, "parse_single_example"):
    serialized = ops.convert_to_tensor(serialized)
    serialized_shape = serialized.get_shape()
    if serialized_shape.ndims is not None:
      if serialized_shape.ndims != 0:
        raise ValueError("Input serialized must be a scalar")
    else:
      serialized = control_flow_ops.with_dependencies(
          [logging_ops.Assert(
              math_ops.equal(array_ops.rank(serialized), 0),
              ["Input serialized must be a scalar"],
              name="SerializedIsScalar")],
          serialized,
          name="SerializedDependencies")
    serialized = array_ops.expand_dims(serialized, 0)
    if names is not None:
      names = ops.convert_to_tensor(names)
      names_shape = names.get_shape()
      if names_shape.ndims is not None:
        if names_shape.ndims != 0:
          raise ValueError("Input names must be a scalar")
      else:
        names = control_flow_ops.with_dependencies(
            [logging_ops.Assert(
                math_ops.equal(array_ops.rank(names), 0),
                ["Input names must be a scalar"],
                name="NamesIsScalar")],
            names,
            name="NamesDependencies")
      names = array_ops.expand_dims(names, 0)

    outputs = parse_example(serialized,
                            names=names,
                            sparse_keys=sparse_keys,
                            sparse_types=sparse_types,
                            dense_keys=dense_keys,
                            dense_types=dense_types,
                            dense_defaults=dense_defaults,
                            dense_shapes=dense_shapes,
                            name=name)
    if dense_keys is not None:
      for d in dense_keys:
        outputs[d] = array_ops.squeeze(outputs[d], [0], name="Squeeze_%s" % d)
    if sparse_keys is not None:
      for s in sparse_keys:
        outputs[s] = ops.SparseTensor(
            array_ops.slice(outputs[s].indices,
                            [0, 1], [-1, -1], name="Slice_Indices_%s" % s),
            outputs[s].values,
            array_ops.slice(outputs[s].shape,
                            [1], [-1], name="Squeeze_Shape_%s" % s))
    return outputs


@ops.RegisterShape("ParseExample")
def _ParseExampleShape(op):
  """Shape function for the ParseExample op."""
  input_shape = op.inputs[0].get_shape().with_rank(1)
  op.inputs[1].get_shape().with_rank(1)  # names
  num_sparse = op.get_attr("Nsparse")
  num_dense = op.get_attr("Ndense")
  dense_shapes = op.get_attr("dense_shapes")
  sparse_index_shapes = [
      tensor_shape.matrix(None, 2) for _ in range(num_sparse)]
  sparse_value_shapes = [tensor_shape.vector(None) for _ in range(num_sparse)]
  sparse_shape_shapes = [tensor_shape.vector(2) for _ in range(num_sparse)]
  assert num_dense == len(dense_shapes)
  dense_shapes = [
      input_shape.concatenate(dense_shape)
      for dense_shape in dense_shapes]
  return (sparse_index_shapes + sparse_value_shapes + sparse_shape_shapes +
          dense_shapes)


def parse_single_sequence_example(serialized,  # pylint: disable=invalid-name
                                  context_sparse_keys=None,
                                  context_sparse_types=None,
                                  context_dense_keys=None,
                                  context_dense_types=None,
                                  context_dense_defaults=None,
                                  context_dense_shapes=None,
                                  feature_list_dense_keys=None,
                                  feature_list_dense_types=None,
                                  feature_list_dense_shapes=None,
                                  feature_list_dense_defaults=None,
                                  debug_name=None,
                                  name="ParseSingleSequenceExample"):
# pylint: disable=line-too-long
  """Parses a single `SequenceExample` proto.

  Parses a single serialized [`SequenceExample`]
  (https://tensorflow.googlesource.com/tensorflow/+/master/tensorflow/core/example/example.proto)
  proto given in `serialized`.

  This op parses a serialize sequence example into a tuple of dictionaries
  mapping keys to `Tensor` and `SparseTensor` objects respectively.
  The first dictionary contains mappings for keys appearing in
  `context_sparse_keys` or `context_dense_keys`, and the second dictionary
  contains mappings for keys appearing in `feature_list_dense_keys`.

  The `context` keys are associated with a `SequenceExample` as a whole,
  independent of time / frame.  In contrast, the `feature_list` keys provide
  a way to access variable-length data within the `FeatureList` section of the
  `SequenceExample` proto.  While the shapes of `context` values are fixed
  with respect to frame, the frame dimension (the first dimension)
  of `feature_list` values may vary from `SequenceExample` to `SequenceExample`
  and even between `feature_list` keys within the same `SequenceExample`.

  The key `context_dense_keys[j]` is mapped to a `Tensor` of type
  `context_dense_types[j]` and of shape `context_dense_shapes[j]`.

  `context_dense_defaults` provides defaults for values referenced using
  `context_dense_keys`.  If a key is not present in this dictionary, the
  corresponding context_dense `Feature` is required in `serialized`.

  `context_dense_shapes[j]` provides the shape of each context `Feature` entry
  referenced by `context_dense_keys[j]`. The number of elements in the
  `Feature` corresponding to `context_dense_key[j]` must always have
  `np.prod(context_dense_shapes[j])` entries. The returned `Tensor` for
  `context_dense_key[j]` has shape `context_dense_shape[j]`.

  The key `context_sparse_keys[j]` is mapped to a `SparseTensor` of type
  `context_sparse_types[j]`. This `SparseTensor` represents a ragged vector.
  Its indices are `[index]`, where `index` is the value's index in the list of
  values associated with that feature and example.

  The key `feature_list_dense_keys[j]` is mapped to a `Tensor` of type
  `feature_list_dense_types[j]` and of shape
  `(T,) + feature_list_dense_shapes[j]`, where `T` is the length of the
  associated `FeatureList` in the `SequenceExample`.

  Note: every key declared in `feature_list_dense_keys` **must** be
  provided in the `SequenceExample`'s `FeatureLists`, even if just empty.
  Exceptions are allowed by adding the given key to the map
  `feature_list_dense_defaults` with value None.  Any key with value None
  map will be  treated as empty (zero length) if not found in the
  `FeatureList` map.

  `debug_name` may contain a descriptive name for the corresponding serialized
  proto. This may be useful for debugging purposes, but it has no effect on the
  output. If not `None`, `debug_name` must be a scalar.

  Args:
    serialized: A scalar (0-D Tensor) of type string, a single binary
      serialized `SequenceExample` proto.
    context_sparse_keys: A list of string keys in the `SequenceExample`'s
      features.  The results for these keys will be returned as
      `SparseTensor` objects.
    context_sparse_types: A list of `DTypes`, the same length as `sparse_keys`.
      Only `tf.float32` (`FloatList`), `tf.int64` (`Int64List`),
      and `tf.string` (`BytesList`) are supported.
    context_dense_keys: A list of string keys in the examples' features.
      The results for these keys will be returned as `Tensor`s
    context_dense_types: A list of DTypes, same length as `context_dense_keys`.
      Only `tf.float32` (`FloatList`), `tf.int64` (`Int64List`),
      and `tf.string` (`BytesList`) are supported.
    context_dense_defaults: A dict mapping string keys to `Tensor`s.
      The keys of the dict must match the context_dense_keys of the feature.
    context_dense_shapes: A list of tuples, same length as `context_dense_keys`.
      The shape of the data for each context_dense feature referenced by
      `context_dense_keys`.  Required for any input tensors identified by
      `context_dense_keys` whose shapes are anything other than `[]` or `[1]`.
    feature_list_dense_keys: A list of string keys in the `SequenceExample`'s
      features_lists. The results for these keys will be returned as `Tensor`s.
    feature_list_dense_types: A list of `DTypes`, same length as
      `feature_list_dense_keys`.  Only `tf.float32` (`FloatList`),
      `tf.int64` (`Int64List`), and `tf.string` (`BytesList`) are supported.
    feature_list_dense_shapes: A list of tuples, same length as
      `feature_list_dense_keys`.  The shape of the data for each
      `FeatureList` feature referenced by `feature_list_dense_keys`.
    feature_list_dense_defaults: A dict mapping key strings to values.
      The only currently allowed value is `None`.  Any key appearing
      in this dict with value `None` is allowed  to be missing from the
      `SequenceExample`.  If missing, the key is treated as zero-length.
    debug_name: A scalar (0-D Tensor) of strings (optional), the name of
      the serialized proto.
    name: A name for this operation (optional).

  Returns:
    A tuple of two `dict`s, each mapping keys to `Tensor`s and `SparseTensor`s.
    The first dict contains the context key/values.
    The second dict contains the feature_list key/values.

  Raises:
    ValueError: If context_sparse and context_dense key sets intersect,
      if input lengths do not match up, or if a value in
      feature_list_dense_defaults is not None.
    TypeError: if feature_list_dense_defaults is not either None or a dict.
  """
# pylint: enable=line-too-long
  with ops.op_scope(
      [serialized, debug_name], name, "parse_single_sequence_example"):
    context_dense_defaults = (
        {} if context_dense_defaults is None else context_dense_defaults)
    context_sparse_keys = (
        [] if context_sparse_keys is None else context_sparse_keys)
    context_sparse_types = (
        [] if context_sparse_types is None else context_sparse_types)
    context_dense_keys = (
        [] if context_dense_keys is None else context_dense_keys)
    context_dense_types = (
        [] if context_dense_types is None else context_dense_types)
    context_dense_shapes = (
        [[]] * len(context_dense_keys)
        if context_dense_shapes is None else context_dense_shapes)
    feature_list_dense_keys = (
        [] if feature_list_dense_keys is None else feature_list_dense_keys)
    feature_list_dense_types = (
        [] if feature_list_dense_types is None else feature_list_dense_types)
    feature_list_dense_shapes = (
        [[]] * len(feature_list_dense_keys)
        if feature_list_dense_shapes is None else feature_list_dense_shapes)
    feature_list_dense_defaults = (
        dict() if feature_list_dense_defaults is None
        else feature_list_dense_defaults)

    # Internal
    feature_list_dense_missing_assumed_empty = []

    num_context_dense = len(context_dense_keys)
    num_feature_list_dense = len(feature_list_dense_keys)
    num_context_sparse = len(context_sparse_keys)

    if len(context_dense_shapes) != num_context_dense:
      raise ValueError(
          "len(context_dense_shapes) != len(context_dense_keys): %d vs. %d"
          % (len(context_dense_shapes), num_context_dense))
    if len(context_dense_types) != num_context_dense:
      raise ValueError(
          "len(context_dense_types) != len(num_context_dense): %d vs. %d"
          % (len(context_dense_types), num_context_dense))
    if len(feature_list_dense_shapes) != num_feature_list_dense:
      raise ValueError(
          "len(feature_list_dense_shapes) != len(feature_list_dense_keys): "
          "%d vs. %d" % (len(feature_list_dense_shapes),
                         num_feature_list_dense))
    if len(feature_list_dense_types) != num_feature_list_dense:
      raise ValueError(
          "len(feature_list_dense_types) != len(num_feature_list_dense):"
          "%d vs. %d" % (len(feature_list_dense_types), num_feature_list_dense))
    if len(context_sparse_types) != num_context_sparse:
      raise ValueError(
          "len(context_sparse_types) != len(context_sparse_keys): %d vs. %d"
          % (len(context_sparse_types), num_context_sparse))
    if num_context_dense + num_context_sparse + num_feature_list_dense == 0:
      raise ValueError(
          "Must provide at least one context_sparse key, context_dense key, "
          "or feature_list_dense key")
    if not set(context_dense_keys).isdisjoint(set(context_sparse_keys)):
      raise ValueError(
          "Context_Dense and context_sparse keys must not intersect; "
          "intersection: %s" %
          set(context_dense_keys).intersection(set(context_sparse_keys)))
    if not isinstance(feature_list_dense_defaults, dict):
      raise TypeError("feature_list_dense_defaults must be a dict")
    for k, v in feature_list_dense_defaults.items():
      if v is not None:
        raise ValueError("Value feature_list_dense_defaults[%s] must be None"
                         % k)
      feature_list_dense_missing_assumed_empty.append(k)

    context_dense_defaults_vec = []
    for i, key in enumerate(context_dense_keys):
      default_value = context_dense_defaults.get(key)
      if default_value is None:
        default_value = constant_op.constant([], dtype=context_dense_types[i])
      elif not isinstance(default_value, ops.Tensor):
        key_name = "key_" + re.sub("[^A-Za-z0-9_.\\-/]", "_", key)
        default_value = ops.convert_to_tensor(
            default_value, dtype=context_dense_types[i], name=key_name)
        default_value = array_ops.reshape(
            default_value, context_dense_shapes[i])

      context_dense_defaults_vec.append(default_value)

    context_dense_shapes = [tensor_util.MakeTensorShapeProto(shape)
                            if isinstance(shape, (list, tuple)) else shape
                            for shape in context_dense_shapes]
    feature_list_dense_shapes = [tensor_util.MakeTensorShapeProto(shape)
                                 if isinstance(shape, (list, tuple)) else shape
                                 for shape in feature_list_dense_shapes]

    outputs = gen_parsing_ops._parse_single_sequence_example(
        serialized=serialized,
        debug_name=debug_name,
        context_dense_defaults=context_dense_defaults_vec,
        context_sparse_keys=context_sparse_keys,
        context_sparse_types=context_sparse_types,
        context_dense_keys=context_dense_keys,
        context_dense_shapes=context_dense_shapes,
        feature_list_dense_keys=feature_list_dense_keys,
        feature_list_dense_types=feature_list_dense_types,
        feature_list_dense_shapes=feature_list_dense_shapes,
        feature_list_dense_missing_assumed_empty=(
            feature_list_dense_missing_assumed_empty),
        name=name)

    (context_sparse_indices, context_sparse_values,
     context_sparse_shapes, context_dense_values,
     feature_list_dense_values) = outputs

    context_sparse_tensors = [
        ops.SparseTensor(ix, val, shape) for (ix, val, shape)
        in zip(context_sparse_indices,
               context_sparse_values,
               context_sparse_shapes)]

    context_output = dict(
        zip(context_sparse_keys + context_dense_keys,
            context_sparse_tensors + context_dense_values))
    feature_list_output = dict(
        zip(feature_list_dense_keys,
            feature_list_dense_values))

    return (context_output, feature_list_output)


@ops.RegisterShape("ParseSingleSequenceExample")
def _ParseSingleSequenceExampleShape(op):
  """Shape function for the ParseExample op."""
  op.inputs[0].get_shape().with_rank(0)  # input
  op.inputs[-1].get_shape().with_rank(0)  # debug_name
  # feature_list_dense_missing_assumed_empty
  op.inputs[1].get_shape().with_rank(1)
  num_context_sparse = op.get_attr("Ncontext_sparse")
  num_context_dense = op.get_attr("Ncontext_dense")
  num_feature_list_dense = op.get_attr("Nfeature_list_dense")
  context_dense_shapes = op.get_attr("context_dense_shapes")
  feature_list_dense_shapes = op.get_attr("feature_list_dense_shapes")
  context_sparse_index_shapes = [
      tensor_shape.matrix(None, 1) for _ in range(num_context_sparse)]
  context_sparse_value_shapes = [
      tensor_shape.vector(None) for _ in range(num_context_sparse)]
  context_sparse_shape_shapes = [
      tensor_shape.vector(1) for _ in range(num_context_sparse)]
  context_dense_shapes = [
      tensor_shape.TensorShape(dense_shape)
      for dense_shape in context_dense_shapes]
  feature_list_dense_shapes = [
      tensor_shape.vector(None).concatenate(dense_shape)
      for dense_shape in feature_list_dense_shapes]
  assert num_context_dense == len(context_dense_shapes)
  assert num_feature_list_dense == len(feature_list_dense_shapes)
  return (context_sparse_index_shapes + context_sparse_value_shapes +
          context_sparse_shape_shapes + context_dense_shapes +
          feature_list_dense_shapes)


ops.RegisterShape("StringToNumber")(
    common_shapes.unchanged_shape)


@ops.RegisterShape("DecodeRaw")
def _DecodeRawShape(op):
  """Shape function for the DecodeRaw op."""
  # NOTE(mrry): Last dimension is data-dependent.
  return [op.inputs[0].get_shape().concatenate([None])]


@ops.RegisterShape("DecodeCSV")
def _DecodeCSVShape(op):
  """Shape function for the DecodeCSV op."""
  input_shape = op.inputs[0].get_shape()
  # Optionally check that all of other inputs are scalar or empty.
  for default_input in op.inputs[1:]:
    default_input_shape = default_input.get_shape().with_rank(1)
    if default_input_shape[0] > 1:
      raise ValueError(
          "Shape of a default must be a length-0 or length-1 vector.")
  return [input_shape] * len(op.outputs)
