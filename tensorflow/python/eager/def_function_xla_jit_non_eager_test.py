# Copyright 2019 The TensorFlow Authors. All Rights Reserved.
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

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.eager import def_function
from tensorflow.python.framework import ops
from tensorflow.python.platform import test


class DefFunctionTest(test.TestCase):

  def testExperimentalCompileNonEager(self):
    with self.assertRaisesRegexp(RuntimeError, "only supported in eager mode"):
      @def_function.function(experimental_compile=True)
      def f(x):
        x = ops.convert_to_tensor(x)
      f(2.0)


if __name__ == "__main__":
  ops.disable_eager_execution()
  test.main()
