# Copyright 2017 The TensorFlow Authors. All Rights Reserved.
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
"""Ops for memory statistics."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.contrib.memory_stats.ops import gen_memory_stats_ops
from tensorflow.contrib.util import loader
from tensorflow.python.platform import resource_loader

_memory_stats_ops_so = loader.load_op_library(
    resource_loader.get_path_to_datafile("_memory_stats_ops.so"))


def BytesInUse():
  """Generates an op that computes the current memory of a device."""
  return gen_memory_stats_ops.bytes_in_use()


def BytesLimit():
  """Generates an op that measures the total memory (in bytes) of a device."""
  return gen_memory_stats_ops.bytes_limit()


def MaxBytesInUse():
  """Generates an op that computes the peak memory of a device."""
  return gen_memory_stats_ops.max_bytes_in_use()
