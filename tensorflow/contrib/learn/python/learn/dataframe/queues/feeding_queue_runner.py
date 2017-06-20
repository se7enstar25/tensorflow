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

"""A `QueueRunner` that takes a feed function as an argument."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.estimator.inputs.queues.feeding_queue_runner import _FeedingQueueRunner
from tensorflow.python.util.deprecation import deprecated


class FeedingQueueRunner(_FeedingQueueRunner):

  @deprecated('2017-06-15', 'Moved to tf.contrib.training.FeedingQueueRunner.')
  def __init__(self, *args, **kwargs):
    super(FeedingQueueRunner, self).__init__(*args, **kwargs)
