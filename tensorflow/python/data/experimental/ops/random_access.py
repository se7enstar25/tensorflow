# Copyright 2021 The TensorFlow Authors. All Rights Reserved.
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
"""Python API for random indexing into a dataset."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.data.util import structure
from tensorflow.python.ops import gen_experimental_dataset_ops


def at(dataset, index):
  """Returns the element at a specific index in a datasest.

  Args:
    dataset: A `tf.data.Dataset` to determine whether it supports random access.
    index: The index at which to fetch the element.

  Returns:
      A (nested) structure of values matching `tf.data.Dataset.element_spec`.

   Raises:
     UnimplementedError: If random access is not yet supported for a dataset.
  """
  # pylint: disable=protected-access
  return structure.from_tensor_list(
      dataset.element_spec,
      gen_experimental_dataset_ops.get_element_at_index(
          dataset,
          index,
          output_types=dataset._flat_types,
          output_shapes=dataset._flat_shapes))
