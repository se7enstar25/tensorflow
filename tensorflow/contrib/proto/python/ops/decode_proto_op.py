# =============================================================================
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
# =============================================================================

# pylint: disable=wildcard-import,unused-import
"""Protocol Buffer decoding from tensors."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.contrib.proto.python.ops.gen_decode_proto_op import decode_proto_v2 as decode_proto
from tensorflow.python.framework import ops
ops.NotDifferentiable("DecodeProtoV2")
