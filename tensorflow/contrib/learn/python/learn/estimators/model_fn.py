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

"""Classes and methods related to model_fn."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import collections

import six

from tensorflow.contrib import framework as contrib_framework
from tensorflow.contrib.framework import get_graph_from_inputs

from tensorflow.python.framework import ops
from tensorflow.python.framework import tensor_shape
from tensorflow.python.ops import array_ops


class ModeKeys(object):
  """Standard names for model modes.

  The following standard keys are defined:

  * `TRAIN`: training mode.
  * `EVAL`: evaluation mode.
  * `INFER`: inference mode.
  """

  TRAIN = 'train'
  EVAL = 'eval'
  INFER = 'infer'


# TODO(roumposg): Pass output_signature_fn instead of signature_fn.
class ModelFnOps(collections.namedtuple(
    'ModelFnOps',
    ['predictions', 'loss', 'train_op', 'eval_metric_ops', 'signature_fn'])):
  """Ops returned from a model_fn."""

  def __new__(cls, mode, predictions=None, loss=None, train_op=None,
              eval_metric_ops=None, signature_fn=None):
    """Creates a validated `ModelFnOps` instance.

    Args:
      mode: One of `ModeKeys`. Specifies if this training, evaluation or
        prediction.
      predictions: Predictions `Tensor` or dict of `Tensor`.
      loss: Training loss `Tensor`.
      train_op: Op for the training step.
      eval_metric_ops: Dict of metric results keyed by name. The values of the
        dict are the results of calling a metric function, such as `Tensor`.
      signature_fn: The signature_fn used for exporting.

    Returns:
      A validated `ModelFnOps` object.

    Raises:
      ValueError: If validation fails.
    """
    # Assert all ops are from the same graph.
    get_graph_from_inputs((predictions, loss, train_op))

    # Validate train_op.
    if train_op is None:
      if mode == ModeKeys.TRAIN:
        raise ValueError('Missing training_op.')
    elif not isinstance(train_op, ops.Operation):
      # TODO(ptucker): Should this be allowed? Consider raising error.
      train_op = ops.convert_to_tensor(train_op).op

    # Validate loss.
    if loss is None:
      if mode in (ModeKeys.TRAIN, ModeKeys.EVAL):
        raise ValueError('Missing loss.')
    else:
      loss = ops.convert_to_tensor(loss)
      loss_shape = loss.get_shape()
      if loss_shape.num_elements() not in (None, 1):
        raise ValueError('Loss must be scalar: %s.' % loss)
      if not loss_shape.is_compatible_with(tensor_shape.scalar()):
        loss = array_ops.reshape(loss, [])

    # Validate predictions.
    if predictions is None:
      if mode == ModeKeys.INFER or mode == ModeKeys.EVAL:
        raise ValueError('Missing predictions.')
    else:
      if isinstance(predictions, dict):
        predictions = {
            k: contrib_framework.convert_to_tensor_or_sparse_tensor(v)
            for k, v in six.iteritems(predictions)
        }
      else:
        predictions = contrib_framework.convert_to_tensor_or_sparse_tensor(
            predictions)

    # Validate eval_metric_ops
    if eval_metric_ops is None:
      eval_metric_ops = {}
    else:
      if not isinstance(eval_metric_ops, dict):
        raise ValueError('eval_metric_ops must be a dict.')

    # validate signature_fn
    if signature_fn:
      if not callable(signature_fn):
        raise ValueError('signature_fn is not callable.')

    return super(ModelFnOps, cls).__new__(cls, predictions, loss, train_op,
                                          eval_metric_ops, signature_fn)
