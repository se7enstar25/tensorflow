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
"""Run the python doc generator and fail if there are any broken links."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os

import tensorflow as tf
from tensorflow.python import debug as tf_debug
from tensorflow.python.platform import googletest
from tensorflow.python.platform import resource_loader
from tensorflow.tools.docs import generate_lib


class Flags(object):
  resource_root = resource_loader.get_root_dir_with_all_resources()
  src_dir = os.path.join(resource_root, 'third_party/tensorflow/docs_src')
  base_dir = os.path.join(resource_root, 'third_party/tensorflow/')
  output_dir = googletest.GetTempDir()


class BuildDocsTest(googletest.TestCase):

  def testBuildDocs(self):
    doc_generator = generate_lib.DocGenerator()

    doc_generator.set_py_modules([('tf', tf), ('tfdbg', tf_debug)])

    status = doc_generator.build(Flags())

    if status:
      self.fail('Found %s Errors!' % status)


if __name__ == '__main__':
  googletest.main()
