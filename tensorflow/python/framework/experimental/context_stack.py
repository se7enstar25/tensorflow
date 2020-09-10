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
"""Thread-local context manager stack."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import contextlib
import threading


# TODO(srbs): Move this to C++.
class _ThreadLocalStack(threading.local):
  """A thread-local stack of objects for providing implicit defaults."""

  def __init__(self):
    super(_ThreadLocalStack, self).__init__()
    self._stack = []

  def peek(self):
    return self._stack[-1] if self._stack else None

  def push(self, ctx):
    return self._stack.append(ctx)

  def pop(self):
    self._stack.pop()


_default_ctx_stack = _ThreadLocalStack()


def get_default():
  """Returns the default execution context."""
  return _default_ctx_stack.peek()


@contextlib.contextmanager
def set_default(ctx):
  """Returns a contextmanager with `ctx` as the default execution context."""
  try:
    _default_ctx_stack.push(ctx)
    yield
  finally:
    _default_ctx_stack.pop()
