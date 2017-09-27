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
"""Tests of utilities for creating input_fns."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.contrib.learn.python.learn.utils import input_fn_utils
from tensorflow.python.framework import dtypes
from tensorflow.python.ops import array_ops
from tensorflow.python.platform import test

class InputFnTest(test.TestCase):

  def test_build_default_serving_input_fn_name(self):
    """Test case for #12755"""
    f = {
        'feature':
            array_ops.placeholder(
                name='feature', shape=[32], dtype=dtypes.float32)
    }
    serving_input = input_fn_utils.build_default_serving_input_fn(f)
    v = serving_input()
    self.assertTrue(isinstance(v, input_fn_utils.InputFnOps))

if __name__ == "__main__":
  test.main()
