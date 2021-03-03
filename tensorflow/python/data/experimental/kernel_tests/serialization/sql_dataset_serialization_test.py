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
"""Tests for checkpointing the SqlDataset."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os

from absl.testing import parameterized

from tensorflow.python.data.experimental.kernel_tests import sql_dataset_test_base
from tensorflow.python.data.experimental.ops import readers
from tensorflow.python.data.kernel_tests import checkpoint_test_base
from tensorflow.python.data.kernel_tests import test_base
from tensorflow.python.framework import combinations
from tensorflow.python.framework import dtypes
from tensorflow.python.ops import array_ops
from tensorflow.python.platform import test


class SqlDatasetCheckpointTest(sql_dataset_test_base.SqlDatasetTestBase,
                               checkpoint_test_base.CheckpointTestBase,
                               parameterized.TestCase):

  def _build_dataset(self, num_repeats):
    data_source_name = os.path.join(test.get_temp_dir(), "tftest.sqlite")
    driver_name = array_ops.placeholder_with_default(
        array_ops.constant("sqlite", dtypes.string), shape=[])
    query = ("SELECT first_name, last_name, motto FROM students ORDER BY "
             "first_name DESC")
    output_types = (dtypes.string, dtypes.string, dtypes.string)
    return readers.SqlDataset(driver_name, data_source_name, query,
                              output_types).repeat(num_repeats)

  @combinations.generate(test_base.default_test_combinations())
  def testCore(self):
    num_repeats = 4
    num_outputs = num_repeats * 2
    self.run_core_tests(lambda: self._build_dataset(num_repeats), num_outputs)


if __name__ == "__main__":
  test.main()
