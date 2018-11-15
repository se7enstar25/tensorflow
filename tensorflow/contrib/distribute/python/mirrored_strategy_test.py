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
# ==============================================================================
"""Tests for class MirroredStrategy."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.contrib.distribute.python import mirrored_strategy
from tensorflow.contrib.distribute.python import strategy_test_lib
from tensorflow.python.eager import context
from tensorflow.python.eager import test
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import ops
from tensorflow.python.framework import test_util
from tensorflow.python.ops import variable_scope
from tensorflow.python.training import distribution_strategy_context as ds_context


class MirroredOneCPUDistributionTest(strategy_test_lib.DistributionTestBase):

  def _get_distribution_strategy(self):
    return mirrored_strategy.MirroredStrategy(["/device:CPU:0"])

  def testMinimizeLossEager(self):
    self._test_minimize_loss_eager(self._get_distribution_strategy())

  def testMinimizeLossGraph(self):
    self._test_minimize_loss_graph(self._get_distribution_strategy())

  def testReplicaId(self):
    self._test_replica_id(self._get_distribution_strategy())

  @test_util.run_in_graph_and_eager_modes
  def testCallAndMergeExceptions(self):
    self._test_call_and_merge_exceptions(self._get_distribution_strategy())

  @test_util.run_in_graph_and_eager_modes
  def testInputContextPropertyLocal(self):
    d = mirrored_strategy.MirroredStrategy(num_gpus_per_worker=2)
    input_fn = self._input_fn_to_test_input_context(
        expected_num_replicas_in_sync=2,
        expected_num_input_pipelines=1,
        expected_input_pipeline_id=0)
    d.make_input_fn_iterator(input_fn)

  def testInputContextPropertyMultiWorker(self):
    d = mirrored_strategy.MirroredStrategy(num_gpus_per_worker=2)
    cluster_spec = {"worker": ["worker1", "worker2", "worker3"]}
    d.configure(cluster_spec=cluster_spec)
    with context.graph_mode():
      # `expected_input_pipeline_id` is None because the input_fn will be called
      # multiple times, each with a different input_pipeline_id.
      input_fn = self._input_fn_to_test_input_context(
          expected_num_replicas_in_sync=6,
          expected_num_input_pipelines=3,
          expected_input_pipeline_id=None)
      d.make_input_fn_iterator(input_fn)


class VariableCreatorStackTest(test.TestCase):

  def testCreatorStacksAreThreadLocal(self):
    devices = ["/device:CPU:0", "/device:GPU:0"]
    dist = mirrored_strategy.MirroredStrategy(devices)

    def model_fn():
      replica_id_str = str(self.evaluate(_replica_id()))

      def thread_creator_fn(next_creator, *args, **kwargs):
        return next_creator(*args, **kwargs) + ":thread_" + replica_id_str

      with variable_scope.variable_creator_scope(thread_creator_fn):
        # Create a variable in this scope.
        v = variable_scope.variable(1.0)

        # This will pause the current thread, and execute the other thread.
        ds_context.get_replica_context().merge_call(lambda _: _)
      return v

    def main_thread_creator(next_creator, *args, **kwargs):
      # We are not using the underlying next_creator for test purposes.
      del next_creator, args, kwargs
      return "main_thread"

    with context.graph_mode(), \
        dist.scope(), \
        variable_scope.variable_creator_scope(main_thread_creator):
      result = dist.call_for_each_replica(model_fn)
      result = dist.unwrap(result)
      expected = ["main_thread:thread_0", "main_thread:thread_1"]
      self.assertEqual(expected, result)


def _replica_id():
  # TODO(cjfj): Return `replica_id_...` directly, once it is a `Tensor`.
  return constant_op.constant(
      ds_context.get_replica_context().replica_id_in_sync_group)


class MultiWorkerMirroredStrategyTest(test.TestCase):

  def testDeviceScope(self):
    """Test the device scope of multi-worker MirroredStrategy."""
    with context.graph_mode():
      strategy = mirrored_strategy.MirroredStrategy(num_gpus=context.num_gpus())
      strategy.configure(
          cluster_spec={"worker": ["/job:worker/task:0", "/job:worker/task:1"]})
      with strategy.scope():
        a = constant_op.constant(1.)
        with ops.device("/cpu:0"):
          b = constant_op.constant(1.)
        self.assertEqual(a.device, "/job:worker/task:0")
        self.assertEqual(b.device, "/job:worker/task:0/device:CPU:0")


if __name__ == "__main__":
  test.main()
