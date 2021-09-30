# Copyright 2020 The TensorFlow Authors. All Rights Reserved.
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
"""Including this as a dependency will result in Tensorflow tests using TFRT.

This function is defined by default in eager/context.py to False. The context
then attempts to import this module. If this file is made available through the
BUILD rule, then this function is overridden and will instead cause
Tensorflow eager execution to run with TFRT.
"""


def is_tfrt_enabled():
  """Returns true to state TFRT should be enabled for Tensorflow tests."""
  return True
