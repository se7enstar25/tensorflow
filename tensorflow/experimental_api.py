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

# Bring in all of the public TensorFlow interface into this
# module.

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

# pylint: disable=g-bad-import-order
from tensorflow.python import pywrap_tensorflow  # pylint: disable=unused-import
# pylint: disable=wildcard-import
from tensorflow.tools.api.generator.api import *  # pylint: disable=redefined-builtin
# pylint: enable=wildcard-import

from tensorflow.python.util.lazy_loader import LazyLoader
contrib = LazyLoader('contrib', globals(), 'tensorflow.contrib')
del LazyLoader

from tensorflow.python.platform import flags  # pylint: disable=g-import-not-at-top
app.flags = flags  # pylint: disable=undefined-variable

del absolute_import
del division
del print_function
