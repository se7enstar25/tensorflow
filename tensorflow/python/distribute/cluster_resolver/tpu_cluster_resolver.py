# Copyright 2017 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Shim so that direct imports of tpu_cluster_resolver get correct symbols.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.distribute.cluster_resolver.tpu.tpu_cluster_resolver import is_running_in_gce  # pylint: disable=unused-import
from tensorflow.python.distribute.cluster_resolver.tpu.tpu_cluster_resolver import TPUClusterResolver
from tensorflow.python.util.tf_export import tf_export

tf_export('distribute.cluster_resolver.TPUClusterResolver')(TPUClusterResolver)
