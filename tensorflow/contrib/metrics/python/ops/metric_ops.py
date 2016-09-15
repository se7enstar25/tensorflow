# Copyright 2016 The TensorFlow Authors. All Rights Reserved.
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
"""Contains metric-computing operations on streamed tensors.

Module documentation, including "@@" callouts, should be put in
third_party/tensorflow/contrib/metrics/__init__.py
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.contrib.framework import deprecated_args
from tensorflow.contrib.framework.python.ops import variables as contrib_variables
from tensorflow.contrib.metrics.python.ops import confusion_matrix_ops
from tensorflow.contrib.metrics.python.ops import metric_ops_util
from tensorflow.contrib.metrics.python.ops import set_ops
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import ops
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import check_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import nn
from tensorflow.python.ops import sparse_ops
from tensorflow.python.ops import state_ops
from tensorflow.python.ops import variable_scope
from tensorflow.python.ops import variables
from tensorflow.python.util.all_util import make_all


IGNORE_MASK_DATE = '2016-10-19'
IGNORE_MASK_INSTRUCTIONS = (
    '`ignore_mask` is being deprecated. Instead use `weights` with values 0.0 '
    'and 1.0 to mask values. For example, `weights=tf.logical_not(mask)`.')


def _mask_weights(mask=None, weights=None):
  """Mask a given set of weights.

  Elements are included when the corresponding `mask` element is `False`, and
  excluded otherwise.

  Args:
    mask: An optional, `bool` `Tensor`.
    weights: An optional `Tensor` whose shape matches `mask` if `mask` is not
      `None`.

  Returns:
    Masked weights if `mask` and `weights` are not `None`, weights equivalent to
    `mask` if `weights` is `None`, and otherwise `weights`.

  Raises:
    ValueError: If `weights` and `mask` are not `None` and have mismatched
      shapes.
  """
  if mask is not None:
    check_ops.assert_type(mask, dtypes.bool)
    if weights is None:
      weights = array_ops.ones_like(mask, dtype=dtypes.float32)
    weights = math_ops.cast(math_ops.logical_not(mask), weights.dtype) * weights

  return weights


def _create_local(name, shape=None, collections=None, dtype=dtypes.float32):
  """Creates a new local variable.

  Args:
    name: The name of the new or existing variable.
    shape: Shape of the new or existing variable.
    collections: A list of collection names to which the Variable will be added.
    dtype: Data type of the variables.

  Returns:
    The created variable.
  """
  # Make sure local variables are added to tf.GraphKeys.LOCAL_VARIABLES
  collections = list(collections or [])
  collections += [ops.GraphKeys.LOCAL_VARIABLES]
  return variables.Variable(
      initial_value=array_ops.zeros(shape, dtype=dtype),
      name=name,
      trainable=False,
      collections=collections)


def _count_condition(values, weights=None, metrics_collections=None,
                     updates_collections=None):
  """Sums the weights of cases where the given values are True.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    values: A `bool` `Tensor` of arbitrary size.
    weights: An optional `Tensor` whose shape matches `values`.
    metrics_collections: An optional list of collections that the metric
      value variable should be added to.
    updates_collections: An optional list of collections that the metric update
      ops should be added to.

  Returns:
    value_tensor: A tensor representing the current value of the metric.
    update_op: An operation that accumulates the error from a batch of data.

  Raises:
    ValueError: If `weights` is not `None` and its shape doesn't match `values`,
      or if either `metrics_collections` or `updates_collections` are not a list
      or tuple.
  """
  check_ops.assert_type(values, dtypes.bool)
  count = _create_local('count', shape=[])

  values = math_ops.to_float(values)
  if weights is not None:
    values.get_shape().assert_is_compatible_with(weights.get_shape())
    weights = math_ops.to_float(weights)
    values = math_ops.mul(values, weights)

  value_tensor = array_ops.identity(count)
  update_op = state_ops.assign_add(count, math_ops.reduce_sum(values))

  if metrics_collections:
    ops.add_to_collections(metrics_collections, value_tensor)

  if updates_collections:
    ops.add_to_collections(updates_collections, update_op)

  return value_tensor, update_op


def _streaming_true_negatives(predictions, labels, weights=None,
                              metrics_collections=None,
                              updates_collections=None,
                              name=None):
  """Sum the weights of true_negatives.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: The predicted values, a `bool` `Tensor` of arbitrary
      dimensions.
    labels: The ground truth values, a `bool` `Tensor` whose dimensions must
      match `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that the metric
      value variable should be added to.
    updates_collections: An optional list of collections that the metric update
      ops should be added to.
    name: An optional variable_scope name.

  Returns:
    value_tensor: A tensor representing the current value of the metric.
    update_op: An operation that accumulates the error from a batch of data.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  with variable_scope.variable_scope(
      [predictions, labels], name, 'true_negatives'):

    predictions.get_shape().assert_is_compatible_with(labels.get_shape())
    is_true_negative = math_ops.logical_and(math_ops.equal(labels, 0),
                                            math_ops.equal(predictions, 0))
    return _count_condition(is_true_negative, weights, metrics_collections,
                            updates_collections)


def _streaming_true_positives(predictions, labels, weights=None,
                              metrics_collections=None,
                              updates_collections=None,
                              name=None):
  """Sum the weights of true_positives.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: The predicted values, a `bool` `Tensor` of arbitrary
      dimensions.
    labels: The ground truth values, a `bool` `Tensor` whose dimensions must
      match `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that the metric
      value variable should be added to.
    updates_collections: An optional list of collections that the metric update
      ops should be added to.
    name: An optional variable_scope name.

  Returns:
    value_tensor: A tensor representing the current value of the metric.
    update_op: An operation that accumulates the error from a batch of data.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  with variable_scope.variable_scope(
      name, 'true_positives', [predictions, labels]):

    predictions.get_shape().assert_is_compatible_with(labels.get_shape())
    is_true_positive = math_ops.logical_and(math_ops.equal(labels, 1),
                                            math_ops.equal(predictions, 1))
    return _count_condition(is_true_positive, weights, metrics_collections,
                            updates_collections)


def _streaming_false_positives(predictions, labels, weights=None,
                               metrics_collections=None,
                               updates_collections=None,
                               name=None):
  """Sum the weights of false positives.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: The predicted values, a `bool` `Tensor` of arbitrary
      dimensions.
    labels: The ground truth values, a `bool` `Tensor` whose dimensions must
      match `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that the metric
      value variable should be added to.
    updates_collections: An optional list of collections that the metric update
      ops should be added to.
    name: An optional variable_scope name.

  Returns:
    value_tensor: A tensor representing the current value of the metric.
    update_op: An operation that accumulates the error from a batch of data.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  with variable_scope.variable_scope(
      name, 'false_positives', [predictions, labels]):

    predictions.get_shape().assert_is_compatible_with(labels.get_shape())
    is_false_positive = math_ops.logical_and(math_ops.equal(labels, 0),
                                             math_ops.equal(predictions, 1))
    return _count_condition(is_false_positive, weights, metrics_collections,
                            updates_collections)


def _streaming_false_negatives(predictions, labels, weights=None,
                               metrics_collections=None,
                               updates_collections=None,
                               name=None):
  """Computes the total number of false positives.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: The predicted values, a `bool` `Tensor` of arbitrary
      dimensions.
    labels: The ground truth values, a `bool` `Tensor` whose dimensions must
      match `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that the metric
      value variable should be added to.
    updates_collections: An optional list of collections that the metric update
      ops should be added to.
    name: An optional variable_scope name.

  Returns:
    value_tensor: A tensor representing the current value of the metric.
    update_op: An operation that accumulates the error from a batch of data.

  Raises:
    ValueError: If `weights` is not `None` and its shape doesn't match `values`,
      or if either `metrics_collections` or `updates_collections` are not a list
      or tuple.
  """
  with variable_scope.variable_scope(
      name, 'false_negatives', [predictions, labels]):

    predictions.get_shape().assert_is_compatible_with(labels.get_shape())
    is_false_negative = math_ops.logical_and(math_ops.equal(labels, 1),
                                             math_ops.equal(predictions, 0))
    return _count_condition(is_false_negative, weights, metrics_collections,
                            updates_collections)


def streaming_mean(values, weights=None, metrics_collections=None,
                   updates_collections=None, name=None):
  """Computes the (weighted) mean of the given values.

  The `streaming_mean` function creates two local variables, `total` and `count`
  that are used to compute the average of `values`. This average is ultimately
  returned as `mean` which is an idempotent operation that simply divides
  `total` by `count`.

  For estimation of the metric  over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the `mean`.
  `update_op` increments `total` with the reduced sum of the product of `values`
  and `weights`, and it increments `count` with the reduced sum of `weights`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    values: A `Tensor` of arbitrary dimensions.
    weights: An optional `Tensor` whose shape matches `values`.
    metrics_collections: An optional list of collections that `mean`
      should be added to.
    updates_collections: An optional list of collections that `update_op`
      should be added to.
    name: An optional variable_scope name.

  Returns:
    mean: A tensor representing the current mean, the value of `total` divided
      by `count`.
    update_op: An operation that increments the `total` and `count` variables
      appropriately and whose value matches `mean_value`.

  Raises:
    ValueError: If `weights` is not `None` and its shape doesn't match `values`,
      or if either `metrics_collections` or `updates_collections` are not a list
      or tuple.
  """
  with variable_scope.variable_scope(name, 'mean', [values, weights]):
    values = math_ops.to_float(values)

    total = _create_local('total', shape=[])
    count = _create_local('count', shape=[])

    if weights is not None:
      values.get_shape().assert_is_compatible_with(weights.get_shape())
      weights = math_ops.to_float(weights)
      values = math_ops.mul(values, weights)
      num_values = math_ops.reduce_sum(weights)
    else:
      num_values = math_ops.to_float(array_ops.size(values))

    total_compute_op = state_ops.assign_add(total, math_ops.reduce_sum(values))
    count_compute_op = state_ops.assign_add(count, num_values)

    def compute_mean(total, count, name):
      return math_ops.select(math_ops.greater(count, 0),
                             math_ops.div(total, count),
                             0, name)

    mean = compute_mean(total, count, 'value')
    with ops.control_dependencies([total_compute_op, count_compute_op]):
      update_op = compute_mean(total, count, 'update_op')

    if metrics_collections:
      ops.add_to_collections(metrics_collections, mean)

    if updates_collections:
      ops.add_to_collections(updates_collections, update_op)

    return mean, update_op


def streaming_mean_tensor(values, weights=None, metrics_collections=None,
                          updates_collections=None, name=None):
  """Computes the element-wise (weighted) mean of the given tensors.

  In contrast to the `streaming_mean` function which returns a scalar with the
  mean,  this function returns an average tensor with the same shape as the
  input tensors.

  The `streaming_mean_tensor` function creates two local variables,
  `total_tensor` and `count_tensor` that are used to compute the average of
  `values`. This average is ultimately returned as `mean` which is an idempotent
  operation that simply divides `total` by `count`.

  For estimation of the metric  over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the `mean`.
  `update_op` increments `total` with the reduced sum of the product of `values`
  and `weights`, and it increments `count` with the reduced sum of `weights`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    values: A `Tensor` of arbitrary dimensions.
    weights: An optional `Tensor` whose shape matches `values`.
    metrics_collections: An optional list of collections that `mean`
      should be added to.
    updates_collections: An optional list of collections that `update_op`
      should be added to.
    name: An optional variable_scope name.

  Returns:
    mean: A float tensor representing the current mean, the value of `total`
      divided by `count`.
    update_op: An operation that increments the `total` and `count` variables
      appropriately and whose value matches `mean_value`.

  Raises:
    ValueError: If `weights` is not `None` and its shape doesn't match `values`,
      or if either `metrics_collections` or `updates_collections` are not a list
      or tuple.
  """
  with variable_scope.variable_scope(name, 'mean', [values, weights]):
    total = _create_local('total_tensor', shape=values.get_shape())
    count = _create_local('count_tensor', shape=values.get_shape())

    if weights is not None:
      values.get_shape().assert_is_compatible_with(weights.get_shape())
      weights = math_ops.to_float(weights)
      values = math_ops.mul(values, weights)
      num_values = weights
    else:
      num_values = array_ops.ones_like(values)

    total_compute_op = state_ops.assign_add(total, values)
    count_compute_op = state_ops.assign_add(count, num_values)

    def compute_mean(total, count, name):
      non_zero_count = math_ops.maximum(count,
                                        array_ops.ones_like(count),
                                        name=name)
      return math_ops.truediv(total, non_zero_count, name=name)

    mean = compute_mean(total, count, 'value')
    with ops.control_dependencies([total_compute_op, count_compute_op]):
      update_op = compute_mean(total, count, 'update_op')

    if metrics_collections:
      ops.add_to_collections(metrics_collections, mean)

    if updates_collections:
      ops.add_to_collections(updates_collections, update_op)

    return mean, update_op


def streaming_accuracy(predictions, labels, weights=None,
                       metrics_collections=None, updates_collections=None,
                       name=None):
  """Calculates how often `predictions` matches `labels`.

  The `streaming_accuracy` function creates two local variables, `total` and
  `count` that are used to compute the frequency with which `predictions`
  matches `labels`. This frequency is ultimately returned as `accuracy`: an
  idempotent operation that simply divides `total` by `count`.

  For estimation of the metric  over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the `accuracy`.
  Internally, an `is_correct` operation computes a `Tensor` with elements 1.0
  where the corresponding elements of `predictions` and `labels` match and 0.0
  otherwise. Then `update_op` increments `total` with the reduced sum of the
  product of `weights` and `is_correct`, and it increments `count` with the
  reduced sum of `weights`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: The predicted values, a `Tensor` of any shape.
    labels: The ground truth values, a `Tensor` whose shape matches
      `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that `accuracy` should
      be added to.
    updates_collections: An optional list of collections that `update_op` should
      be added to.
    name: An optional variable_scope name.

  Returns:
    accuracy: A tensor representing the accuracy, the value of `total` divided
      by `count`.
    update_op: An operation that increments the `total` and `count` variables
      appropriately and whose value matches `accuracy`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  predictions, labels = metric_ops_util.remove_squeezable_dimensions(
      predictions, labels)
  predictions.get_shape().assert_is_compatible_with(labels.get_shape())
  if labels.dtype != predictions.dtype:
    predictions = math_ops.cast(predictions, labels.dtype)
  is_correct = math_ops.to_float(math_ops.equal(predictions, labels))
  return streaming_mean(is_correct, weights, metrics_collections,
                        updates_collections, name or 'accuracy')


@deprecated_args(IGNORE_MASK_DATE, IGNORE_MASK_INSTRUCTIONS, 'ignore_mask')
def streaming_precision(predictions, labels, ignore_mask=None, weights=None,
                        metrics_collections=None, updates_collections=None,
                        name=None):
  """Computes the precision of the predictions with respect to the labels.

  The `streaming_precision` function creates two local variables,
  `true_positives` and `false_positives`, that are used to compute the
  precision. This value is ultimately returned as `precision`, an idempotent
  operation that simply divides `true_positives` by the sum of `true_positives`
  and `false_positives`.

  For estimation of the metric  over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `precision`. `update_op` weights each prediction by the corresponding value in
  `weights`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.
  Alternatively, if `ignore_mask` is not `None`, then mask values where
  `ignore_mask` is `True`.

  Args:
    predictions: The predicted values, a `bool` `Tensor` of arbitrary shape.
    labels: The ground truth values, a `bool` `Tensor` whose dimensions must
      match `predictions`.
    ignore_mask: An optional, `bool` `Tensor` whose shape matches `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that `precision` should
      be added to.
    updates_collections: An optional list of collections that `update_op` should
      be added to.
    name: An optional variable_scope name.

  Returns:
    precision: Scalar float `Tensor` with the value of `true_positives`
      divided by the sum of `true_positives` and `false_positives`.
    update_op: `Operation` that increments `true_positives` and
      `false_positives` variables appropriately and whose value matches
      `precision`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `ignore_mask` is not `None` and its shape doesn't match `predictions`, or
      if `weights` is not `None` and its shape doesn't match `predictions`, or
      if either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  with variable_scope.variable_scope(
      name, 'precision', [predictions, labels]):

    predictions, labels = metric_ops_util.remove_squeezable_dimensions(
        predictions, labels)
    predictions.get_shape().assert_is_compatible_with(labels.get_shape())

    weights = _mask_weights(ignore_mask, weights)
    true_positives, true_positives_update_op = _streaming_true_positives(
        predictions, labels, weights, metrics_collections=None,
        updates_collections=None, name=None)
    false_positives, false_positives_update_op = _streaming_false_positives(
        predictions, labels, weights, metrics_collections=None,
        updates_collections=None, name=None)

    def compute_precision(name):
      return math_ops.select(
          math_ops.greater(true_positives + false_positives, 0),
          math_ops.div(true_positives, true_positives + false_positives),
          0,
          name)

    precision = compute_precision('value')
    with ops.control_dependencies([true_positives_update_op,
                                   false_positives_update_op]):
      update_op = compute_precision('update_op')

    if metrics_collections:
      ops.add_to_collections(metrics_collections, precision)

    if updates_collections:
      ops.add_to_collections(updates_collections, update_op)

    return precision, update_op


@deprecated_args(IGNORE_MASK_DATE, IGNORE_MASK_INSTRUCTIONS, 'ignore_mask')
def streaming_recall(predictions, labels, ignore_mask=None, weights=None,
                     metrics_collections=None, updates_collections=None,
                     name=None):
  """Computes the recall of the predictions with respect to the labels.

  The `streaming_recall` function creates two local variables, `true_positives`
  and `false_negatives`, that are used to compute the recall. This value is
  ultimately returned as `recall`, an idempotent operation that simply divides
  `true_positives` by the sum of `true_positives`  and `false_negatives`.

  For estimation of the metric  over a stream of data, the function creates an
  `update_op` that updates these variables and returns the `recall`. `update_op`
  weights each prediction by the corresponding value in `weights`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.
  Alternatively, if `ignore_mask` is not `None`, then mask values where
  `ignore_mask` is `True`.

  Args:
    predictions: The predicted values, a `bool` `Tensor` of arbitrary shape.
    labels: The ground truth values, a `bool` `Tensor` whose dimensions must
      match `predictions`.
    ignore_mask: An optional, `bool` `Tensor` whose shape matches `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that `recall` should
      be added to.
    updates_collections: An optional list of collections that `update_op` should
      be added to.
    name: An optional variable_scope name.

  Returns:
    recall: Scalar float `Tensor` with the value of `true_positives` divided
      by the sum of `true_positives` and `false_negatives`.
    update_op: `Operation` that increments `true_positives` and
      `false_negatives` variables appropriately and whose value matches
      `recall`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `ignore_mask` is not `None` and its shape doesn't match `predictions`, or
      if `weights` is not `None` and its shape doesn't match `predictions`, or
      if either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  with variable_scope.variable_scope(name, 'recall', [predictions, labels]):
    predictions, labels = metric_ops_util.remove_squeezable_dimensions(
        predictions, labels)
    predictions.get_shape().assert_is_compatible_with(labels.get_shape())

    weights = _mask_weights(ignore_mask, weights)
    true_positives, true_positives_update_op = _streaming_true_positives(
        predictions, labels, weights, metrics_collections=None,
        updates_collections=None, name=None)
    false_negatives, false_negatives_update_op = _streaming_false_negatives(
        predictions, labels, weights, metrics_collections=None,
        updates_collections=None, name=None)

    def compute_recall(true_positives, false_negatives, name):
      return math_ops.select(
          math_ops.greater(true_positives + false_negatives, 0),
          math_ops.div(true_positives, true_positives + false_negatives),
          0,
          name)

    recall = compute_recall(true_positives, false_negatives, 'value')
    with ops.control_dependencies([true_positives_update_op,
                                   false_negatives_update_op]):
      update_op = compute_recall(true_positives, false_negatives, 'update_op')

    if metrics_collections:
      ops.add_to_collections(metrics_collections, recall)

    if updates_collections:
      ops.add_to_collections(updates_collections, update_op)

    return recall, update_op


def _tp_fn_tn_fp(predictions, labels, thresholds, weights=None):
  """Computes true_positives, false_negatives, true_negatives, false_positives.

  The `_tp_fn_tn_fp` function creates four local variables, `true_positives`,
  `true_negatives`, `false_positives` and `false_negatives`.
  `true_positive[i]` is defined as the total weight of values in `predictions`
  above `thresholds[i]` whose corresponding entry in `labels` is `True`.
  `false_negatives[i]` is defined as the total weight of values in `predictions`
  at most `thresholds[i]` whose corresponding entry in `labels` is `True`.
  `true_negatives[i]` is defined as the total weight of values in `predictions`
  at most `thresholds[i]` whose corresponding entry in `labels` is `False`.
  `false_positives[i]` is defined as the total weight of values in `predictions`
  above `thresholds[i]` whose corresponding entry in `labels` is `False`.

  For estimation of these metrics over a stream of data, for each metric the
  function respectively creates an `update_op` operation that updates the
  variable and returns its value.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: A floating point `Tensor` of arbitrary shape and whose values
      are in the range `[0, 1]`.
    labels: A `bool` `Tensor` whose shape matches `predictions`.
    thresholds: A python list or tuple of float thresholds in `[0, 1]`.
    weights: An optional `Tensor` whose shape matches `predictions`.

  Returns:
    true_positive: A variable of shape [len(thresholds)].
    false_negative: A variable of shape [len(thresholds)].
    true_negatives: A variable of shape [len(thresholds)].
    false_positives: A variable of shape [len(thresholds)].
    true_positives_update_op: An operation that increments the `true_positives`.
    false_negative_update_op: An operation that increments the `false_negative`.
    true_negatives_update_op: An operation that increments the `true_negatives`.
    false_positives_update_op: An operation that increments the
      `false_positives`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`.
  """
  predictions, labels = metric_ops_util.remove_squeezable_dimensions(
      predictions, labels)
  predictions.get_shape().assert_is_compatible_with(labels.get_shape())

  num_thresholds = len(thresholds)

  # Reshape predictions and labels
  predictions = array_ops.reshape(predictions, [-1, 1])
  labels = array_ops.reshape(math_ops.cast(labels, dtype=dtypes.bool), [1, -1])

  # Use static shape if known.
  num_predictions = predictions.get_shape().as_list()[0]

  # Otherwise use dynamic shape.
  if num_predictions is None:
    num_predictions = array_ops.shape(predictions)[0]
  thresh_tiled = array_ops.tile(
      array_ops.expand_dims(array_ops.constant(thresholds), [1]),
      array_ops.pack([1, num_predictions]))

  # Tile the predictions after thresholding them across different thresholds.
  pred_is_pos = math_ops.greater(
      array_ops.tile(array_ops.transpose(predictions), [num_thresholds, 1]),
      thresh_tiled)
  pred_is_neg = math_ops.logical_not(pred_is_pos)

  # Tile labels by number of thresholds
  label_is_pos = array_ops.tile(labels, [num_thresholds, 1])
  label_is_neg = math_ops.logical_not(label_is_pos)

  true_positives = _create_local('true_positives', shape=[num_thresholds])
  false_negatives = _create_local('false_negatives', shape=[num_thresholds])
  true_negatives = _create_local('true_negatives', shape=[num_thresholds])
  false_positives = _create_local('false_positives', shape=[num_thresholds])

  is_true_positive = math_ops.to_float(
      math_ops.logical_and(label_is_pos, pred_is_pos))
  is_false_negative = math_ops.to_float(
      math_ops.logical_and(label_is_pos, pred_is_neg))
  is_false_positive = math_ops.to_float(
      math_ops.logical_and(label_is_neg, pred_is_pos))
  is_true_negative = math_ops.to_float(
      math_ops.logical_and(label_is_neg, pred_is_neg))

  if weights is not None:
    weights = math_ops.to_float(weights)
    weights_tiled = array_ops.tile(
        array_ops.reshape(weights, [1, -1]), [num_thresholds, 1])
    thresh_tiled.get_shape().assert_is_compatible_with(
        weights_tiled.get_shape())
    is_true_positive *= weights_tiled
    is_false_negative *= weights_tiled
    is_false_positive *= weights_tiled
    is_true_negative *= weights_tiled

  true_positives_update_op = state_ops.assign_add(
      true_positives, math_ops.reduce_sum(is_true_positive, 1))
  false_negatives_update_op = state_ops.assign_add(
      false_negatives, math_ops.reduce_sum(is_false_negative, 1))
  true_negatives_update_op = state_ops.assign_add(
      true_negatives, math_ops.reduce_sum(is_true_negative, 1))
  false_positives_update_op = state_ops.assign_add(
      false_positives, math_ops.reduce_sum(is_false_positive, 1))

  return (true_positives, false_negatives, true_negatives, false_positives,
          true_positives_update_op, false_negatives_update_op,
          true_negatives_update_op, false_positives_update_op)


def streaming_auc(predictions, labels, weights=None, num_thresholds=200,
                  metrics_collections=None, updates_collections=None,
                  curve='ROC', name=None):
  """Computes the approximate AUC via a Riemann sum.

  The `streaming_auc` function creates four local variables, `true_positives`,
  `true_negatives`, `false_positives` and `false_negatives` that are used to
  compute the AUC. To discretize the AUC curve, a linearly spaced set of
  thresholds is used to compute pairs of recall and precision values. The area
  under the ROC-curve is therefore computed using the height of the recall
  values by the false positive rate, while the area under the PR-curve is the
  computed using the height of the precision values by the recall.

  This value is ultimately returned as `auc`, an idempotent operation that
  computes the area under a discretized curve of precision versus recall values
  (computed using the afformentioned variables). The `num_thresholds` variable
  controls the degree of discretization with larger numbers of thresholds more
  closely approximating the true AUC.

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the `auc`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: A floating point `Tensor` of arbitrary shape and whose values
      are in the range `[0, 1]`.
    labels: A `bool` `Tensor` whose shape matches `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    num_thresholds: The number of thresholds to use when discretizing the roc
      curve.
    metrics_collections: An optional list of collections that `auc` should be
      added to.
    updates_collections: An optional list of collections that `update_op` should
      be added to.
    curve: Specifies the name of the curve to be computed, 'ROC' [default] or
    'PR' for the Precision-Recall-curve.
    name: An optional variable_scope name.

  Returns:
    auc: A scalar tensor representing the current area-under-curve.
    update_op: An operation that increments the `true_positives`,
      `true_negatives`, `false_positives` and `false_negatives` variables
      appropriately and whose value matches `auc`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  with variable_scope.variable_scope(name, 'auc', [predictions, labels]):
    if curve != 'ROC' and  curve != 'PR':
      raise ValueError('curve must be either ROC or PR, %s unknown' %
                       (curve))
    kepsilon = 1e-7  # to account for floating point imprecisions
    thresholds = [(i + 1) * 1.0 / (num_thresholds - 1)
                  for i in range(num_thresholds-2)]
    thresholds = [0.0 - kepsilon] + thresholds + [1.0 + kepsilon]

    (tp, fn, tn, fp, tp_update_op, fn_update_op, tn_update_op,
     fp_update_op) = _tp_fn_tn_fp(predictions, labels, thresholds, weights)

    # Add epsilons to avoid dividing by 0.
    epsilon = 1.0e-6
    assert array_ops.squeeze(fp).get_shape().as_list()[0] == num_thresholds

    def compute_auc(tp, fn, tn, fp, name):
      """Computes the roc-auc or pr-auc based on confusion counts."""
      recall = math_ops.div(tp + epsilon, tp + fn + epsilon)
      if curve == 'ROC':
        fp_rate = math_ops.div(fp, fp + tn + epsilon)
        x = fp_rate
        y = recall
      else:  # curve == 'PR'.
        precision = math_ops.div(tp + epsilon, tp + fp + epsilon)
        x = recall
        y = precision
      return math_ops.reduce_sum(math_ops.mul(
          x[:num_thresholds - 1] - x[1:],
          (y[:num_thresholds - 1] + y[1:]) / 2.), name=name)

    # sum up the areas of all the trapeziums
    auc = compute_auc(tp, fn, tn, fp, 'value')
    update_op = compute_auc(
        tp_update_op, fn_update_op, tn_update_op, fp_update_op, 'update_op')

    if metrics_collections:
      ops.add_to_collections(metrics_collections, auc)

    if updates_collections:
      ops.add_to_collections(updates_collections, update_op)

    return auc, update_op


def streaming_specificity_at_sensitivity(
    predictions, labels, sensitivity, weights=None, num_thresholds=200,
    metrics_collections=None, updates_collections=None, name=None):
  """Computes the the specificity at a given sensitivity.

  The `streaming_specificity_at_sensitivity` function creates four local
  variables, `true_positives`, `true_negatives`, `false_positives` and
  `false_negatives` that are used to compute the specificity at the given
  sensitivity value. The threshold for the given sensitivity value is computed
  and used to evaluate the corresponding specificity.

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `specificity`. `update_op` increments the `true_positives`, `true_negatives`,
  `false_positives` and `false_negatives` counts with the weight of each case
  found in the `predictions` and `labels`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  For additional information about specificity and sensitivity, see the
  following: https://en.wikipedia.org/wiki/Sensitivity_and_specificity

  Args:
    predictions: A floating point `Tensor` of arbitrary shape and whose values
      are in the range `[0, 1]`.
    labels: A `bool` `Tensor` whose shape matches `predictions`.
    sensitivity: A scalar value in range `[0, 1]`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    num_thresholds: The number of thresholds to use for matching the given
      sensitivity.
    metrics_collections: An optional list of collections that `specificity`
      should be added to.
    updates_collections: An optional list of collections that `update_op` should
      be added to.
    name: An optional variable_scope name.

  Returns:
    specificity: A scalar tensor representing the specificity at the given
      `specificity` value.
    update_op: An operation that increments the `true_positives`,
      `true_negatives`, `false_positives` and `false_negatives` variables
      appropriately and whose value matches `specificity`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      `sensitivity` is not between 0 and 1, or if either `metrics_collections`
      or `updates_collections` are not a list or tuple.
  """
  if sensitivity < 0 or sensitivity > 1:
    raise ValueError('`sensitivity` must be in the range [0, 1].')

  with variable_scope.variable_scope(name, 'specificity_at_sensitivity',
                                     [predictions, labels]):
    kepsilon = 1e-7  # to account for floating point imprecisions
    thresholds = [(i + 1) * 1.0 / (num_thresholds - 1)
                  for i in range(num_thresholds-2)]
    thresholds = [0.0 - kepsilon] + thresholds + [1.0 - kepsilon]

    (tp, fn, tn, fp, tp_update_op, fn_update_op, tn_update_op,
     fp_update_op) = _tp_fn_tn_fp(predictions, labels, thresholds, weights)

    assert array_ops.squeeze(fp).get_shape().as_list()[0] == num_thresholds

    def compute_specificity_at_sensitivity(name):
      """Computes the specificity at the given sensitivity.

      Args:
        name: The name of the operation.

      Returns:
        The specificity using the aggregated values.
      """
      sensitivities = math_ops.div(tp, tp + fn + kepsilon)

      # We'll need to use this trick until tf.argmax allows us to specify
      # whether we should use the first or last index in case of ties.
      min_val = math_ops.reduce_min(math_ops.abs(sensitivities - sensitivity))
      indices_at_minval = math_ops.equal(
          math_ops.abs(sensitivities - sensitivity), min_val)
      indices_at_minval = math_ops.to_int64(indices_at_minval)
      indices_at_minval = math_ops.cumsum(indices_at_minval)
      tf_index = math_ops.argmax(indices_at_minval, 0)
      tf_index = math_ops.cast(tf_index, dtypes.int32)

      # Now, we have the implicit threshold, so compute the specificity:
      return math_ops.div(tn[tf_index],
                          tn[tf_index] + fp[tf_index] + kepsilon,
                          name)

    specificity = compute_specificity_at_sensitivity('value')
    with ops.control_dependencies(
        [tp_update_op, fn_update_op, tn_update_op, fp_update_op]):
      update_op = compute_specificity_at_sensitivity('update_op')

    if metrics_collections:
      ops.add_to_collections(metrics_collections, specificity)

    if updates_collections:
      ops.add_to_collections(updates_collections, update_op)

    return specificity, update_op


def streaming_sensitivity_at_specificity(
    predictions, labels, specificity, weights=None, num_thresholds=200,
    metrics_collections=None, updates_collections=None, name=None):
  """Computes the the specificity at a given sensitivity.

  The `streaming_sensitivity_at_specificity` function creates four local
  variables, `true_positives`, `true_negatives`, `false_positives` and
  `false_negatives` that are used to compute the sensitivity at the given
  specificity value. The threshold for the given specificity value is computed
  and used to evaluate the corresponding sensitivity.

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `sensitivity`. `update_op` increments the `true_positives`, `true_negatives`,
  `false_positives` and `false_negatives` counts with the weight of each case
  found in the `predictions` and `labels`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  For additional information about specificity and sensitivity, see the
  following: https://en.wikipedia.org/wiki/Sensitivity_and_specificity

  Args:
    predictions: A floating point `Tensor` of arbitrary shape and whose values
      are in the range `[0, 1]`.
    labels: A `bool` `Tensor` whose shape matches `predictions`.
    specificity: A scalar value in range `[0, 1]`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    num_thresholds: The number of thresholds to use for matching the given
      specificity.
    metrics_collections: An optional list of collections that `sensitivity`
      should be added to.
    updates_collections: An optional list of collections that `update_op` should
      be added to.
    name: An optional variable_scope name.

  Returns:
    sensitivity: A scalar tensor representing the sensitivity at the given
      `specificity` value.
    update_op: An operation that increments the `true_positives`,
      `true_negatives`, `false_positives` and `false_negatives` variables
      appropriately and whose value matches `sensitivity`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      `specificity` is not between 0 and 1, or if either `metrics_collections`
      or `updates_collections` are not a list or tuple.
  """
  if specificity < 0 or specificity > 1:
    raise ValueError('`specificity` must be in the range [0, 1].')

  with variable_scope.variable_scope(name, 'sensitivity_at_specificity',
                                     [predictions, labels]):
    kepsilon = 1e-7  # to account for floating point imprecisions
    thresholds = [(i + 1) * 1.0 / (num_thresholds - 1)
                  for i in range(num_thresholds-2)]
    thresholds = [0.0 - kepsilon] + thresholds + [1.0 + kepsilon]

    (tp, fn, tn, fp, tp_update_op, fn_update_op, tn_update_op,
     fp_update_op) = _tp_fn_tn_fp(predictions, labels, thresholds, weights)
    assert array_ops.squeeze(fp).get_shape().as_list()[0] == num_thresholds

    def compute_sensitivity_at_specificity(name):
      specificities = math_ops.div(tn, tn + fp + kepsilon)
      tf_index = math_ops.argmin(math_ops.abs(specificities - specificity), 0)
      tf_index = math_ops.cast(tf_index, dtypes.int32)

      # Now, we have the implicit threshold, so compute the sensitivity:
      return math_ops.div(tp[tf_index],
                          tp[tf_index] + fn[tf_index] + kepsilon,
                          name)

    sensitivity = compute_sensitivity_at_specificity('value')
    with ops.control_dependencies(
        [tp_update_op, fn_update_op, tn_update_op, fp_update_op]):
      update_op = compute_sensitivity_at_specificity('update_op')

    if metrics_collections:
      ops.add_to_collections(metrics_collections, sensitivity)

    if updates_collections:
      ops.add_to_collections(updates_collections, update_op)

    return sensitivity, update_op


def streaming_precision_at_thresholds(predictions, labels, thresholds,
                                      weights=None,
                                      metrics_collections=None,
                                      updates_collections=None, name=None):
  """Computes precision values for different `thresholds` on `predictions`.

  The `streaming_precision_at_thresholds` function creates four local variables,
  `true_positives`, `true_negatives`, `false_positives` and `false_negatives`
  for various values of thresholds. `precision[i]` is defined as the total
  weight of values in `predictions` above `thresholds[i]` whose corresponding
  entry in `labels` is `True`, divided by the total weight of values in
  `predictions` above `thresholds[i]` (`true_positives[i] / (true_positives[i] +
  false_positives[i])`).

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `precision`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: A floating point `Tensor` of arbitrary shape and whose values
      are in the range `[0, 1]`.
    labels: A `bool` `Tensor` whose shape matches `predictions`.
    thresholds: A python list or tuple of float thresholds in `[0, 1]`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that `auc` should be
      added to.
    updates_collections: An optional list of collections that `update_op` should
      be added to.
    name: An optional variable_scope name.

  Returns:
    precision: A float tensor of shape [len(thresholds)].
    update_op: An operation that increments the `true_positives`,
      `true_negatives`, `false_positives` and `false_negatives` variables that
      are used in the computation of `precision`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  with variable_scope.variable_scope(name, 'precision_at_thresholds',
                                     [predictions, labels]):

    # TODO(nsilberman): Replace with only tp and fp, this results in unnecessary
    # variable creation. b/30842882
    (true_positives, _, _, false_positives, true_positives_compute_op, _, _,
     false_positives_compute_op,) = _tp_fn_tn_fp(
         predictions, labels, thresholds, weights)

    # avoid division by zero
    epsilon = 1e-7
    def compute_precision(name):
      precision = math_ops.div(true_positives,
                               epsilon + true_positives + false_positives,
                               name='precision_' + name)
      return precision

    precision = compute_precision('value')
    with ops.control_dependencies([true_positives_compute_op,
                                   false_positives_compute_op]):
      update_op = compute_precision('update_op')

    if metrics_collections:
      ops.add_to_collections(metrics_collections, precision)

    if updates_collections:
      ops.add_to_collections(updates_collections, update_op)

    return precision, update_op


def streaming_recall_at_thresholds(predictions, labels, thresholds,
                                   weights=None, metrics_collections=None,
                                   updates_collections=None, name=None):
  """Computes various recall values for different `thresholds` on `predictions`.

  The `streaming_recall_at_thresholds` function creates four local variables,
  `true_positives`, `true_negatives`, `false_positives` and `false_negatives`
  for various values of thresholds. `recall[i]` is defined as the total weight
  of values in `predictions` above `thresholds[i]` whose corresponding entry in
  `labels` is `True`, divided by the total weight of `True` values in `labels`
  (`true_positives[i] / (true_positives[i] + false_negatives[i])`).

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the `recall`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: A floating point `Tensor` of arbitrary shape and whose values
      are in the range `[0, 1]`.
    labels: A `bool` `Tensor` whose shape matches `predictions`.
    thresholds: A python list or tuple of float thresholds in `[0, 1]`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that `recall` should be
      added to.
    updates_collections: An optional list of collections that `update_op` should
      be added to.
    name: An optional variable_scope name.

  Returns:
    recall: A float tensor of shape [len(thresholds)].
    update_op: An operation that increments the `true_positives`,
      `true_negatives`, `false_positives` and `false_negatives` variables that
      are used in the computation of `recall`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  with variable_scope.variable_scope(name, 'recall_at_thresholds',
                                     [predictions, labels]):
    (true_positives, false_negatives, _, _, true_positives_compute_op,
     false_negatives_compute_op, _, _,) = _tp_fn_tn_fp(
         predictions, labels, thresholds, weights)

    # avoid division by zero
    epsilon = 1e-7
    def compute_recall(name):
      recall = math_ops.div(true_positives,
                            epsilon + true_positives + false_negatives,
                            name='recall_' + name)
      return recall

    recall = compute_recall('value')
    with ops.control_dependencies([true_positives_compute_op,
                                   false_negatives_compute_op]):
      update_op = compute_recall('update_op')

    if metrics_collections:
      ops.add_to_collections(metrics_collections, recall)

    if updates_collections:
      ops.add_to_collections(updates_collections, update_op)

    return recall, update_op


@deprecated_args(IGNORE_MASK_DATE, IGNORE_MASK_INSTRUCTIONS, 'ignore_mask')
def streaming_recall_at_k(predictions, labels, k, ignore_mask=None,
                          weights=None, metrics_collections=None,
                          updates_collections=None, name=None):
  """Computes the recall@k of the predictions with respect to dense labels.

  The `streaming_recall_at_k` function creates two local variables, `total` and
  `count`, that are used to compute the recall@k frequency. This frequency is
  ultimately returned as `recall_at_<k>`: an idempotent operation that simply
  divides `total` by `count`.

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `recall_at_<k>`. Internally, an `in_top_k` operation computes a `Tensor` with
  shape [batch_size] whose elements indicate whether or not the corresponding
  label is in the top `k` `predictions`. Then `update_op` increments `total`
  with the reduced sum of `weights` where `in_top_k` is `True`, and it
  increments `count` with the reduced sum of `weights`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.
  Alternatively, if `ignore_mask` is not `None`, then mask values where
  `ignore_mask` is `True`.

  Args:
    predictions: A floating point tensor of dimension [batch_size, num_classes]
    labels: A tensor of dimension [batch_size] whose type is in `int32`,
      `int64`.
    k: The number of top elements to look at for computing recall.
    ignore_mask: An optional, `bool` `Tensor` whose shape matches `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that `recall_at_k`
      should be added to.
    updates_collections: An optional list of collections `update_op` should be
      added to.
    name: An optional variable_scope name.

  Returns:
    recall_at_k: A tensor representing the recall@k, the fraction of labels
      which fall into the top `k` predictions.
    update_op: An operation that increments the `total` and `count` variables
      appropriately and whose value matches `recall_at_k`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `ignore_mask` is not `None` and its shape doesn't match `predictions`, or
      if `weights` is not `None` and its shape doesn't match `predictions`, or
      if either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  in_top_k = math_ops.to_float(nn.in_top_k(predictions, labels, k))
  return streaming_mean(in_top_k,
                        _mask_weights(ignore_mask, weights),
                        metrics_collections,
                        updates_collections,
                        name or ('recall_at_%d' % k))


# TODO(ptucker): Validate range of values in labels?
@deprecated_args(IGNORE_MASK_DATE, IGNORE_MASK_INSTRUCTIONS, 'ignore_mask')
def streaming_sparse_recall_at_k(predictions,
                                 labels,
                                 k,
                                 class_id=None,
                                 ignore_mask=None,
                                 weights=None,
                                 metrics_collections=None,
                                 updates_collections=None,
                                 name=None):
  """Computes recall@k of the predictions with respect to sparse labels.

  If `class_id` is specified, we calculate recall by considering only the
      entries in the batch for which `class_id` is in the label, and computing
      the fraction of them for which `class_id` is in the top-k `predictions`.
  If `class_id` is not specified, we'll calculate recall as how often on
      average a class among the labels of a batch entry is in the top-k
      `predictions`.

  `streaming_sparse_recall_at_k` creates two local variables,
  `true_positive_at_<k>` and `false_negative_at_<k>`, that are used to compute
  the recall_at_k frequency. This frequency is ultimately returned as
  `recall_at_<k>`: an idempotent operation that simply divides
  `true_positive_at_<k>` by total (`true_positive_at_<k>` + `recall_at_<k>`).

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `recall_at_<k>`. Internally, a `top_k` operation computes a `Tensor`
  indicating the top `k` `predictions`. Set operations applied to `top_k` and
  `labels` calculate the true positives and false negatives weighted by
  `weights`. Then `update_op` increments `true_positive_at_<k>` and
  `false_negative_at_<k>` using these values.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.
  Alternatively, if `ignore_mask` is not `None`, then mask values where
  `ignore_mask` is `True`.

  Args:
    predictions: Float `Tensor` with shape [D1, ... DN, num_classes] where
      N >= 1. Commonly, N=1 and predictions has shape [batch size, num_classes].
      The final dimension contains the logit values for each class. [D1, ... DN]
      must match `labels`.
    labels: `int64` `Tensor` or `SparseTensor` with shape
      [D1, ... DN, num_labels], where N >= 1 and num_labels is the number of
      target classes for the associated prediction. Commonly, N=1 and `labels`
      has shape [batch_size, num_labels]. [D1, ... DN] must match `labels`.
      Values should be in range [0, num_classes], where num_classes is the last
      dimension of `predictions`.
    k: Integer, k for @k metric.
    class_id: Integer class ID for which we want binary metrics. This should be
      in range [0, num_classes], where num_classes is the last dimension of
      `predictions`.
    ignore_mask: An optional, `bool` `Tensor` whose shape is broadcastable to
      the the first [D1, ... DN] dimensions of `predictions` and `labels`.
    weights: An optional `Tensor` whose shape is broadcastable to the the first
      [D1, ... DN] dimensions of `predictions` and `labels`.
    metrics_collections: An optional list of collections that values should
      be added to.
    updates_collections: An optional list of collections that updates should
      be added to.
    name: Name of new update operation, and namespace for other dependant ops.

  Returns:
    recall: Scalar `float64` `Tensor` with the value of `true_positives` divided
      by the sum of `true_positives` and `false_negatives`.
    update_op: `Operation` that increments `true_positives` and
      `false_negatives` variables appropriately, and whose value matches
      `recall`.

  Raises:
    ValueError: If `ignore_mask` is not `None` and its shape doesn't match
      `predictions`, or if `weights` is not `None` and its shape doesn't match
      `predictions`, or if either `metrics_collections` or `updates_collections`
      are not a list or tuple.
  """
  default_name = 'recall_at_%d' % k
  if class_id is not None:
    default_name = '%s_class%d' % (default_name, class_id)

  with ops.name_scope(name, default_name, [predictions, labels]) as scope:
    _, top_k_idx = nn.top_k(predictions, k)
    top_k_idx = math_ops.to_int64(top_k_idx)
    weights = _mask_weights(ignore_mask, weights)
    tp, tp_update = _streaming_sparse_true_positive_at_k(
        predictions_idx=top_k_idx, labels=labels, k=k, class_id=class_id,
        weights=weights)
    fn, fn_update = _streaming_sparse_false_negative_at_k(
        predictions_idx=top_k_idx, labels=labels, k=k, class_id=class_id,
        weights=weights)

    metric = math_ops.div(tp, math_ops.add(tp, fn), name=scope)
    update = math_ops.div(
        tp_update, math_ops.add(tp_update, fn_update), name='update')
    if metrics_collections:
      ops.add_to_collections(metrics_collections, metric)
    if updates_collections:
      ops.add_to_collections(updates_collections, update)
    return metric, update


# TODO(ptucker): Validate range of values in labels?
@deprecated_args(IGNORE_MASK_DATE, IGNORE_MASK_INSTRUCTIONS, 'ignore_mask')
def streaming_sparse_precision_at_k(predictions,
                                    labels,
                                    k,
                                    class_id=None,
                                    ignore_mask=None,
                                    weights=None,
                                    metrics_collections=None,
                                    updates_collections=None,
                                    name=None):
  """Computes precision@k of the predictions with respect to sparse labels.

  If `class_id` is specified, we calculate precision by considering only the
      entries in the batch for which `class_id` is in the top-k highest
      `predictions`, and computing the fraction of them for which `class_id` is
      indeed a correct label.
  If `class_id` is not specified, we'll calculate precision as how often on
      average a class among the top-k classes with the highest predicted values
      of a batch entry is correct and can be found in the label for that entry.

  `streaming_sparse_precision_at_k` creates two local variables,
  `true_positive_at_<k>` and `false_positive_at_<k>`, that are used to compute
  the precision@k frequency. This frequency is ultimately returned as
  `precision_at_<k>`: an idempotent operation that simply divides
  `true_positive_at_<k>` by total (`true_positive_at_<k>` +
  `false_positive_at_<k>`).

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `precision_at_<k>`. Internally, a `top_k` operation computes a `Tensor`
  indicating the top `k` `predictions`. Set operations applied to `top_k` and
  `labels` calculate the true positives and false positives weighted by
  `weights`. Then `update_op` increments `true_positive_at_<k>` and
  `false_positive_at_<k>` using these values.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.
  Alternatively, if `ignore_mask` is not `None`, then mask values where
  `ignore_mask` is `True`.

  Args:
    predictions: Float `Tensor` with shape [D1, ... DN, num_classes] where
      N >= 1. Commonly, N=1 and predictions has shape [batch size, num_classes].
      The final dimension contains the logit values for each class. [D1, ... DN]
      must match `labels`.
    labels: `int64` `Tensor` or `SparseTensor` with shape
      [D1, ... DN, num_labels], where N >= 1 and num_labels is the number of
      target classes for the associated prediction. Commonly, N=1 and `labels`
      has shape [batch_size, num_labels]. [D1, ... DN] must match
      `predictions_idx`. Values should be in range [0, num_classes], where
      num_classes is the last dimension of `predictions`.
    k: Integer, k for @k metric.
    class_id: Integer class ID for which we want binary metrics. This should be
      in range [0, num_classes], where num_classes is the last dimension of
      `predictions`.
    ignore_mask: An optional, `bool` `Tensor` whose shape is broadcastable to
      the the first [D1, ... DN] dimensions of `predictions` and `labels`.
    weights: An optional `Tensor` whose shape is broadcastable to the the first
      [D1, ... DN] dimensions of `predictions` and `labels`.
    metrics_collections: An optional list of collections that values should
      be added to.
    updates_collections: An optional list of collections that updates should
      be added to.
    name: Name of new update operation, and namespace for other dependant ops.

  Returns:
    precision: Scalar `float64` `Tensor` with the value of `true_positives`
      divided by the sum of `true_positives` and `false_positives`.
    update_op: `Operation` that increments `true_positives` and
      `false_positives` variables appropriately, and whose value matches
      `precision`.

  Raises:
    ValueError: If `ignore_mask` is not `None` and its shape doesn't match
      `predictions`, or if `weights` is not `None` and its shape doesn't match
      `predictions`, or if either `metrics_collections` or `updates_collections`
      are not a list or tuple.
  """
  default_name = 'precision_at_%d' % k
  if class_id is not None:
    default_name = '%s_class%d' % (default_name, class_id)
  with ops.name_scope(name, default_name, [predictions, labels]) as scope:
    _, top_k_idx = nn.top_k(predictions, k)
    top_k_idx = math_ops.to_int64(top_k_idx)
    weights = _mask_weights(ignore_mask, weights)
    tp, tp_update = _streaming_sparse_true_positive_at_k(
        predictions_idx=top_k_idx, labels=labels, k=k, class_id=class_id,
        weights=weights)
    fp, fp_update = _streaming_sparse_false_positive_at_k(
        predictions_idx=top_k_idx, labels=labels, k=k, class_id=class_id,
        weights=weights)

    metric = math_ops.div(tp, math_ops.add(tp, fp), name=scope)
    update = math_ops.div(
        tp_update, math_ops.add(tp_update, fp_update), name='update')
    if metrics_collections:
      ops.add_to_collections(metrics_collections, metric)
    if updates_collections:
      ops.add_to_collections(updates_collections, update)
    return metric, update


def _select_class_id(ids, selected_id):
  """Filter all but `selected_id` out of `ids`.

  Args:
    ids: `int64` `Tensor` or `SparseTensor` of IDs.
    selected_id: Int id to select.

  Returns:
    `SparseTensor` of same dimensions as `ids`, except for the last dimension,
    which might be smaller. This contains only the entries equal to
    `selected_id`.
  """
  if isinstance(ids, ops.SparseTensor):
    return sparse_ops.sparse_retain(
        ids, math_ops.equal(ids.values, selected_id))

  # TODO(ptucker): Make this more efficient, maybe add a sparse version of
  # tf.equal and tf.reduce_any?

  # Shape of filled IDs is the same as `ids` with the last dim collapsed to 1.
  ids_shape = array_ops.shape(ids)
  ids_last_dim = array_ops.size(ids_shape) - 1
  filled_selected_id_shape = math_ops.reduced_shape(
      ids_shape, array_ops.reshape(ids_last_dim, [1]))

  # Intersect `ids` with the selected ID.
  filled_selected_id = array_ops.fill(
      filled_selected_id_shape, math_ops.to_int64(selected_id))
  return set_ops.set_intersection(filled_selected_id, ids)


def _maybe_select_class_id(labels, predictions_idx, selected_id=None):
  """If class ID is specified, filter all other classes.

  Args:
    labels: `int64` `Tensor` or `SparseTensor` with shape
      [D1, ... DN, num_labels], where N >= 1 and num_labels is the number of
      target classes for the associated prediction. Commonly, N=1 and `labels`
      has shape [batch_size, num_labels]. [D1, ... DN] must match
      `predictions_idx`.
    predictions_idx: `int64` `Tensor` of class IDs, with shape [D1, ... DN, k]
      where N >= 1. Commonly, N=1 and predictions has shape [batch size, k].
    selected_id: Int id to select.

  Returns:
    Tuple of `labels` and `predictions_idx`, possibly with classes removed.
  """
  if selected_id is None:
    return labels, predictions_idx
  return (_select_class_id(labels, selected_id),
          _select_class_id(predictions_idx, selected_id))


def _streaming_sparse_true_positive_at_k(predictions_idx,
                                         labels,
                                         k,
                                         class_id=None,
                                         weights=None,
                                         name=None):
  """Calculates weighted per step true positives for recall@k and precision@k.

  If `class_id` is specified, calculate binary true positives for `class_id`
      only.
  If `class_id` is not specified, calculate metrics for `k` predicted vs
      `n` label classes, where `n` is the 2nd dimension of `labels`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions_idx: 1-D or higher `int64` `Tensor` with last dimension `k`,
      top `k` predicted classes. For rank `n`, the first `n-1` dimensions must
      match `labels`.
    labels: `int64` `Tensor` or `SparseTensor` with shape
      [D1, ... DN, num_labels], where N >= 1 and num_labels is the number of
      target classes for the associated prediction. Commonly, N=1 and `labels`
      has shape [batch_size, num_labels]. [D1, ... DN] must match
      `predictions_idx`.
    k: Integer, k for @k metric. This is only used for default op name.
    class_id: Class for which we want binary metrics.
    weights: `Tensor` whose shape is broadcastable to the the first [D1, ... DN]
      dimensions of `predictions_idx` and `labels`.
    name: Name of new variable, and namespace for other dependant ops.

  Returns:
    A tuple of `Variable` and update `Operation`.

  Raises:
    ValueError: If `weights` is not `None` and has an incomptable shape.
  """
  default_name = 'true_positive_at_%d' % k
  if class_id is not None:
    default_name = '%s_class%d' % (default_name, class_id)
  with ops.name_scope(name, default_name, [predictions_idx, labels]) as scope:
    labels, predictions_idx = _maybe_select_class_id(labels,
                                                     predictions_idx,
                                                     class_id)
    tp = set_ops.set_size(set_ops.set_intersection(predictions_idx, labels))
    tp = math_ops.to_double(tp)
    if weights is not None:
      tp.get_shape().assert_is_compatible_with(weights.get_shape())
      weights = math_ops.to_double(weights)
      tp = math_ops.mul(tp, weights)
    batch_total_tp = math_ops.reduce_sum(tp)

    var = contrib_variables.local_variable(
        array_ops.zeros([], dtype=dtypes.float64), name=scope)
    return var, state_ops.assign_add(var, batch_total_tp, name='update')


def _streaming_sparse_false_positive_at_k(predictions_idx,
                                          labels,
                                          k,
                                          class_id=None,
                                          weights=None,
                                          name=None):
  """Calculates weighted per step false positives for precision@k.

  If `class_id` is specified, calculate binary true positives for `class_id`
      only.
  If `class_id` is not specified, calculate metrics for `k` predicted vs
      `n` label classes, where `n` is the 2nd dimension of `labels`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions_idx: 1-D or higher `int64` `Tensor` with last dimension `k`,
      top `k` predicted classes. For rank `n`, the first `n-1` dimensions must
      match `labels`.
    labels: `int64` `Tensor` or `SparseTensor` with shape
      [D1, ... DN, num_labels], where N >= 1 and num_labels is the number of
      target classes for the associated prediction. Commonly, N=1 and `labels`
      has shape [batch_size, num_labels]. [D1, ... DN] must match
      `predictions_idx`.
    k: Integer, k for @k metric. This is only used for default op name.
    class_id: Class for which we want binary metrics.
    weights: `Tensor` whose shape is broadcastable to the the first [D1, ... DN]
      dimensions of `predictions_idx` and `labels`.
    name: Name of new variable, and namespace for other dependant ops.

  Returns:
    A tuple of `Variable` and update `Operation`.

  Raises:
    ValueError: If `weights` is not `None` and has an incomptable shape.
  """
  default_name = 'false_positive_at_%d' % k
  if class_id is not None:
    default_name = '%s_class%d' % (default_name, class_id)
  with ops.name_scope(name, default_name, [predictions_idx, labels]) as scope:
    labels, predictions_idx = _maybe_select_class_id(labels,
                                                     predictions_idx,
                                                     class_id)
    fp = set_ops.set_size(set_ops.set_difference(predictions_idx,
                                                 labels,
                                                 aminusb=True))
    fp = math_ops.to_double(fp)
    if weights is not None:
      fp.get_shape().assert_is_compatible_with(weights.get_shape())
      weights = math_ops.to_double(weights)
      fp = math_ops.mul(fp, weights)
    batch_total_fp = math_ops.reduce_sum(fp)

    var = contrib_variables.local_variable(
        array_ops.zeros([], dtype=dtypes.float64), name=scope)
    return var, state_ops.assign_add(var, batch_total_fp, name='update')


def _streaming_sparse_false_negative_at_k(predictions_idx,
                                          labels,
                                          k,
                                          class_id=None,
                                          weights=None,
                                          name=None):
  """Calculates weighted per step false negatives for recall@k.

  If `class_id` is specified, calculate binary true positives for `class_id`
      only.
  If `class_id` is not specified, calculate metrics for `k` predicted vs
      `n` label classes, where `n` is the 2nd dimension of `labels`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions_idx: 1-D or higher `int64` `Tensor` with last dimension `k`,
      top `k` predicted classes. For rank `n`, the first `n-1` dimensions must
      match `labels`.
    labels: `int64` `Tensor` or `SparseTensor` with shape
      [D1, ... DN, num_labels], where N >= 1 and num_labels is the number of
      target classes for the associated prediction. Commonly, N=1 and `labels`
      has shape [batch_size, num_labels]. [D1, ... DN] must match
      `predictions_idx`.
    k: Integer, k for @k metric. This is only used for default op name.
    class_id: Class for which we want binary metrics.
    weights: `Tensor` whose shape is broadcastable to the the first [D1, ... DN]
      dimensions of `predictions_idx` and `labels`.
    name: Name of new variable, and namespace for other dependant ops.

  Returns:
    A tuple of `Variable` and update `Operation`.

  Raises:
    ValueError: If `weights` is not `None` and has an incomptable shape.
  """
  default_name = 'false_negative_at_%d' % k
  if class_id is not None:
    default_name = '%s_class%d' % (default_name, class_id)
  with ops.name_scope(name, default_name, [predictions_idx, labels]) as scope:
    labels, predictions_idx = _maybe_select_class_id(labels,
                                                     predictions_idx,
                                                     class_id)
    fn = set_ops.set_size(set_ops.set_difference(predictions_idx,
                                                 labels,
                                                 aminusb=False))
    fn = math_ops.to_double(fn)
    if weights is not None:
      fn.get_shape().assert_is_compatible_with(weights.get_shape())
      weights = math_ops.to_double(weights)
      fn = math_ops.mul(fn, weights)
    batch_total_fn = math_ops.reduce_sum(fn)

    var = contrib_variables.local_variable(
        array_ops.zeros([], dtype=dtypes.float64), name=scope)
    return var, state_ops.assign_add(var, batch_total_fn, name='update')


def streaming_mean_absolute_error(predictions, labels, weights=None,
                                  metrics_collections=None,
                                  updates_collections=None,
                                  name=None):
  """Computes the mean absolute error between the labels and predictions.

  The `streaming_mean_absolute_error` function creates two local variables,
  `total` and `count` that are used to compute the mean absolute error. This
  average is weighted by `weights`, and it is ultimately returned as
  `mean_absolute_error`: an idempotent operation that simply divides `total` by
  `count`.

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `mean_absolute_error`. Internally, an `absolute_errors` operation computes the
  absolute value of the differences between `predictions` and `labels`. Then
  `update_op` increments `total` with the reduced sum of the product of
  `weights` and `absolute_errors`, and it increments `count` with the reduced
  sum of `weights`

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: A `Tensor` of arbitrary shape.
    labels: A `Tensor` of the same shape as `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that
      `mean_absolute_error` should be added to.
    updates_collections: An optional list of collections that `update_op` should
      be added to.
    name: An optional variable_scope name.

  Returns:
    mean_absolute_error: A tensor representing the current mean, the value of
      `total` divided by `count`.
    update_op: An operation that increments the `total` and `count` variables
      appropriately and whose value matches `mean_absolute_error`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  predictions, labels = metric_ops_util.remove_squeezable_dimensions(
      predictions, labels)
  predictions.get_shape().assert_is_compatible_with(labels.get_shape())
  absolute_errors = math_ops.abs(predictions - labels)
  return streaming_mean(absolute_errors, weights, metrics_collections,
                        updates_collections, name or 'mean_absolute_error')


def streaming_mean_relative_error(predictions, labels, normalizer, weights=None,
                                  metrics_collections=None,
                                  updates_collections=None,
                                  name=None):
  """Computes the mean relative error by normalizing with the given values.

  The `streaming_mean_relative_error` function creates two local variables,
  `total` and `count` that are used to compute the mean relative absolute error.
  This average is weighted by `weights`, and it is ultimately returned as
  `mean_relative_error`: an idempotent operation that simply divides `total` by
  `count`.

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `mean_reative_error`. Internally, a `relative_errors` operation divides the
  absolute value of the differences between `predictions` and `labels` by the
  `normalizer`. Then `update_op` increments `total` with the reduced sum of the
  product of `weights` and `relative_errors`, and it increments `count` with the
  reduced sum of `weights`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: A `Tensor` of arbitrary shape.
    labels: A `Tensor` of the same shape as `predictions`.
    normalizer: A `Tensor` of the same shape as `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that
      `mean_relative_error` should be added to.
    updates_collections: An optional list of collections that `update_op` should
      be added to.
    name: An optional variable_scope name.

  Returns:
    mean_relative_error: A tensor representing the current mean, the value of
      `total` divided by `count`.
    update_op: An operation that increments the `total` and `count` variables
      appropriately and whose value matches `mean_relative_error`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  predictions, labels = metric_ops_util.remove_squeezable_dimensions(
      predictions, labels)
  predictions.get_shape().assert_is_compatible_with(labels.get_shape())

  predictions, normalizer = metric_ops_util.remove_squeezable_dimensions(
      predictions, normalizer)
  predictions.get_shape().assert_is_compatible_with(normalizer.get_shape())
  relative_errors = math_ops.select(
      math_ops.equal(normalizer, 0.0),
      array_ops.zeros_like(labels),
      math_ops.div(math_ops.abs(labels - predictions), normalizer))
  return streaming_mean(relative_errors, weights, metrics_collections,
                        updates_collections, name or 'mean_relative_error')


def streaming_mean_squared_error(predictions, labels, weights=None,
                                 metrics_collections=None,
                                 updates_collections=None,
                                 name=None):
  """Computes the mean squared error between the labels and predictions.

  The `streaming_mean_squared_error` function creates two local variables,
  `total` and `count` that are used to compute the mean squared error.
  This average is weighted by `weights`, and it is ultimately returned as
  `mean_squared_error`: an idempotent operation that simply divides `total` by
  `count`.

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `mean_squared_error`. Internally, a `squared_error` operation computes the
  element-wise square of the difference between `predictions` and `labels`. Then
  `update_op` increments `total` with the reduced sum of the product of
  `weights` and `squared_error`, and it increments `count` with the reduced sum
  of `weights`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: A `Tensor` of arbitrary shape.
    labels: A `Tensor` of the same shape as `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that
      `mean_squared_error` should be added to.
    updates_collections: An optional list of collections that `update_op` should
      be added to.
    name: An optional variable_scope name.

  Returns:
    mean_squared_error: A tensor representing the current mean, the value of
      `total` divided by `count`.
    update_op: An operation that increments the `total` and `count` variables
      appropriately and whose value matches `mean_squared_error`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  predictions, labels = metric_ops_util.remove_squeezable_dimensions(
      predictions, labels)
  predictions.get_shape().assert_is_compatible_with(labels.get_shape())
  squared_error = math_ops.square(labels - predictions)
  return streaming_mean(squared_error, weights, metrics_collections,
                        updates_collections, name or 'mean_squared_error')


def streaming_root_mean_squared_error(predictions, labels, weights=None,
                                      metrics_collections=None,
                                      updates_collections=None,
                                      name=None):
  """Computes the root mean squared error between the labels and predictions.

  The `streaming_root_mean_squared_error` function creates two local variables,
  `total` and `count` that are used to compute the root mean squared error.
  This average is weighted by `weights`, and it is ultimately returned as
  `root_mean_squared_error`: an idempotent operation that takes the square root
  of the division of `total` by `count`.

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `root_mean_squared_error`. Internally, a `squared_error` operation computes
  the element-wise square of the difference between `predictions` and `labels`.
  Then `update_op` increments `total` with the reduced sum of the product of
  `weights` and `squared_error`, and it increments `count` with the reduced sum
  of `weights`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: A `Tensor` of arbitrary shape.
    labels: A `Tensor` of the same shape as `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that
      `root_mean_squared_error` should be added to.
    updates_collections: An optional list of collections that `update_op` should
      be added to.
    name: An optional variable_scope name.

  Returns:
    root_mean_squared_error: A tensor representing the current mean, the value
      of `total` divided by `count`.
    update_op: An operation that increments the `total` and `count` variables
      appropriately and whose value matches `root_mean_squared_error`.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  predictions, labels = metric_ops_util.remove_squeezable_dimensions(
      predictions, labels)
  predictions.get_shape().assert_is_compatible_with(labels.get_shape())
  value_tensor, update_op = streaming_mean_squared_error(
      predictions, labels, weights, None, None,
      name or 'root_mean_squared_error')

  root_mean_squared_error = math_ops.sqrt(value_tensor)
  with ops.control_dependencies([update_op]):
    update_op = math_ops.sqrt(update_op)

  if metrics_collections:
    ops.add_to_collections(metrics_collections, root_mean_squared_error)

  if updates_collections:
    ops.add_to_collections(updates_collections, update_op)

  return root_mean_squared_error, update_op


# TODO(nsilberman): add a 'normalized' flag so that the user can request
# normalization if the inputs are not normalized.
def streaming_mean_cosine_distance(predictions, labels, dim, weights=None,
                                   metrics_collections=None,
                                   updates_collections=None,
                                   name=None):
  """Computes the cosine distance between the labels and predictions.

  The `streaming_mean_cosine_distance` function creates two local variables,
  `total` and `count` that are used to compute the average cosine distance
  between `predictions` and `labels`. This average is weighted by `weights`,
  and it is ultimately returned as `mean_distance`, which is an idempotent
  operation that simply divides `total` by `count`.

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `mean_distance`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.

  Args:
    predictions: A `Tensor` of the same shape as `labels`.
    labels: A `Tensor` of arbitrary shape.
    dim: The dimension along which the cosine distance is computed.
    weights: An optional `Tensor` whose shape matches `predictions`, and whose
      dimension `dim` is 1.
    metrics_collections: An optional list of collections that the metric
      value variable should be added to.
    updates_collections: An optional list of collections that the metric update
      ops should be added to.
    name: An optional variable_scope name.

  Returns:
    mean_distance: A tensor representing the current mean, the value of `total`
      divided by `count`.
    update_op: An operation that increments the `total` and `count` variables
      appropriately.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `weights` is not `None` and its shape doesn't match `predictions`, or if
      either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  predictions, labels = metric_ops_util.remove_squeezable_dimensions(
      predictions, labels)
  predictions.get_shape().assert_is_compatible_with(labels.get_shape())
  radial_diffs = math_ops.mul(predictions, labels)
  radial_diffs = math_ops.reduce_sum(radial_diffs,
                                     reduction_indices=[dim,],
                                     keep_dims=True)
  mean_distance, update_op = streaming_mean(radial_diffs, weights,
                                            None,
                                            None,
                                            name or 'mean_cosine_distance')
  mean_distance = math_ops.sub(1.0, mean_distance)
  update_op = math_ops.sub(1.0, update_op)

  if metrics_collections:
    ops.add_to_collections(metrics_collections, mean_distance)

  if updates_collections:
    ops.add_to_collections(updates_collections, update_op)

  return mean_distance, update_op


@deprecated_args(IGNORE_MASK_DATE, IGNORE_MASK_INSTRUCTIONS, 'ignore_mask')
def streaming_percentage_less(values, threshold, ignore_mask=None, weights=None,
                              metrics_collections=None,
                              updates_collections=None,
                              name=None):
  """Computes the percentage of values less than the given threshold.

  The `streaming_percentage_less` function creates two local variables,
  `total` and `count` that are used to compute the percentage of `values` that
  fall below `threshold`. This rate is weighted by `weights`, and it is
  ultimately returned as `percentage` which is an idempotent operation that
  simply divides `total` by `count`.

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the
  `percentage`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.
  Alternatively, if `ignore_mask` is not `None`, then mask values where
  `ignore_mask` is `True`.

  Args:
    values: A numeric `Tensor` of arbitrary size.
    threshold: A scalar threshold.
    ignore_mask: An optional, `bool` `Tensor` whose shape matches `values`.
    weights: An optional `Tensor` whose shape matches `values`.
    metrics_collections: An optional list of collections that the metric
      value variable should be added to.
    updates_collections: An optional list of collections that the metric update
      ops should be added to.
    name: An optional variable_scope name.

  Returns:
    percentage: A tensor representing the current mean, the value of `total`
      divided by `count`.
    update_op: An operation that increments the `total` and `count` variables
      appropriately.

  Raises:
    ValueError: If `ignore_mask` is not `None` and its shape doesn't match
      `values`, or if `weights` is not `None` and its shape doesn't match
      `values`, or if either `metrics_collections` or `updates_collections` are
      not a list or tuple.
  """
  is_below_threshold = math_ops.to_float(math_ops.less(values, threshold))
  return streaming_mean(is_below_threshold, _mask_weights(ignore_mask, weights),
                        metrics_collections,
                        updates_collections,
                        name or 'percentage_below_threshold')


@deprecated_args(IGNORE_MASK_DATE, IGNORE_MASK_INSTRUCTIONS, 'ignore_mask')
def streaming_mean_iou(predictions,
                       labels,
                       num_classes,
                       ignore_mask=None,
                       weights=None,
                       metrics_collections=None,
                       updates_collections=None,
                       name=None):
  """Calculate per-step mean Intersection-Over-Union (mIOU).

  Mean Intersection-Over-Union is a common evaluation metric for
  semantic image segmentation, which first computes the IOU for each
  semantic class and then computes the average over classes.
  IOU is defined as follows:
    IOU = true_positive / (true_positive + false_positive + false_negative).
  The predictions are accumulated in a confusion matrix, weighted by `weights`,
  and mIOU is then calculated from it.

  For estimation of the metric over a stream of data, the function creates an
  `update_op` operation that updates these variables and returns the `mean_iou`.

  If `weights` is `None`, weights default to 1. Use weights of 0 to mask values.
  Alternatively, if `ignore_mask` is not `None`, then mask values where
  `ignore_mask` is `True`.

  Args:
    predictions: A tensor of prediction results for semantic labels, whose
      shape is [batch size] and type `int32` or `int64`. The tensor will be
      flattened, if its rank > 1.
    labels: A tensor of ground truth labels with shape [batch size] and of
      type `int32` or `int64`. The tensor will be flattened, if its rank > 1.
    num_classes: The possible number of labels the prediction task can
      have. This value must be provided, since a confusion matrix of
      dimension = [num_classes, num_classes] will be allocated.
    ignore_mask: An optional, `bool` `Tensor` whose shape matches `predictions`.
    weights: An optional `Tensor` whose shape matches `predictions`.
    metrics_collections: An optional list of collections that `mean_iou`
      should be added to.
    updates_collections: An optional list of collections `update_op` should be
      added to.
    name: An optional variable_scope name.

  Returns:
    mean_iou: A tensor representing the mean intersection-over-union.
    update_op: An operation that increments the confusion matrix.

  Raises:
    ValueError: If `predictions` and `labels` have mismatched shapes, or if
      `ignore_mask` is not `None` and its shape doesn't match `predictions`, or
      if `weights` is not `None` and its shape doesn't match `predictions`, or
      if either `metrics_collections` or `updates_collections` are not a list or
      tuple.
  """
  with variable_scope.variable_scope(name, 'mean_iou', [predictions, labels]):
    # Check if shape is compatible.
    predictions.get_shape().assert_is_compatible_with(labels.get_shape())

    # Local variable to accumulate the predictions in the confusion matrix.
    cm_dtype = dtypes.int64 if weights is not None else dtypes.float64
    total_cm = _create_local('total_confusion_matrix',
                             shape=[num_classes, num_classes], dtype=cm_dtype)

    # Cast the type to int64 required by confusion_matrix_ops.
    predictions = math_ops.to_int64(predictions)
    labels = math_ops.to_int64(labels)
    num_classes = math_ops.to_int64(num_classes)

    # Flatten the input if its rank > 1.
    predictions_rank = predictions.get_shape().ndims
    if predictions_rank > 1:
      predictions = array_ops.reshape(predictions, [-1])

    labels_rank = labels.get_shape().ndims
    if labels_rank > 1:
      labels = array_ops.reshape(labels, [-1])

    weights = _mask_weights(ignore_mask, weights)
    if weights is not None:
      weights_rank = weights.get_shape().ndims
      if weights_rank > 1:
        weights = array_ops.reshape(weights, [-1])

    # Accumulate the prediction to current confusion matrix.
    current_cm = confusion_matrix_ops.confusion_matrix(
        predictions, labels, num_classes, weights=weights, dtype=cm_dtype)
    update_op = state_ops.assign_add(total_cm, current_cm)

    def compute_mean_iou(name):
      """Compute the mean intersection-over-union via the confusion matrix."""
      sum_over_row = math_ops.to_float(math_ops.reduce_sum(total_cm, 0))
      sum_over_col = math_ops.to_float(math_ops.reduce_sum(total_cm, 1))
      cm_diag = math_ops.to_float(array_ops.diag_part(total_cm))
      denominator = sum_over_row + sum_over_col - cm_diag

      # If the value of the denominator is 0, set it to 1 to avoid
      # zero division.
      denominator = math_ops.select(
          math_ops.greater(denominator, 0),
          denominator,
          array_ops.ones_like(denominator))
      iou = math_ops.div(cm_diag, denominator)
      return math_ops.reduce_mean(iou, name=name)

    mean_iou = compute_mean_iou('mean_iou')

    if metrics_collections:
      ops.add_to_collections(metrics_collections, mean_iou)

    if updates_collections:
      ops.add_to_collections(updates_collections, update_op)

    return mean_iou, update_op


def aggregate_metrics(*value_update_tuples):
  """Aggregates the metric value tensors and update ops into two lists.

  Args:
    *value_update_tuples: a variable number of tuples, each of which contain the
      pair of (value_tensor, update_op) from a streaming metric.

  Returns:
    a list of value tensors and a list of update ops.

  Raises:
    ValueError: if `value_update_tuples` is empty.
  """
  if not value_update_tuples:
    raise ValueError('Expected at least one value_tensor/update_op pair')
  value_ops, update_ops = zip(*value_update_tuples)
  return list(value_ops), list(update_ops)


def aggregate_metric_map(names_to_tuples):
  """Aggregates the metric names to tuple dictionary.

  This function is useful for pairing metric names with their associated value
  and update ops when the list of metrics is long. For example:

    metrics_to_values, metrics_to_updates = slim.metrics.aggregate_metric_map({
        'Mean Absolute Error': new_slim.metrics.streaming_mean_absolute_error(
            predictions, labels, weights),
        'Mean Relative Error': new_slim.metrics.streaming_mean_relative_error(
            predictions, labels, labels, weights),
        'RMSE Linear': new_slim.metrics.streaming_root_mean_squared_error(
            predictions, labels, weights),
        'RMSE Log': new_slim.metrics.streaming_root_mean_squared_error(
            predictions, labels, weights),
    })

  Args:
    names_to_tuples: a map of metric names to tuples, each of which contain the
      pair of (value_tensor, update_op) from a streaming metric.

  Returns:
    A dictionary from metric names to value ops and a dictionary from metric
    names to update ops.
  """
  metric_names = names_to_tuples.keys()
  value_ops, update_ops = zip(*names_to_tuples.values())
  return dict(zip(metric_names, value_ops)), dict(zip(metric_names, update_ops))


__all__ = make_all(__name__)
