# Lint as: python3
# Copyright 2020 The TensorFlow Authors. All Rights Reserved.
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
"""Fault tolerance test for parameter server training in TF2."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import gc
import os
import threading
import time

from tensorflow.python.compat import v2_compat
from tensorflow.python.data.ops import dataset_ops
from tensorflow.python.distribute import multi_process_runner
from tensorflow.python.distribute import multi_worker_test_base
from tensorflow.python.distribute import parameter_server_strategy_v2
from tensorflow.python.distribute.cluster_resolver import SimpleClusterResolver
from tensorflow.python.distribute.coordinator import cluster_coordinator
from tensorflow.python.eager import context
from tensorflow.python.eager import def_function
from tensorflow.python.eager import test
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import errors
from tensorflow.python.framework import ops
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import check_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import random_ops
from tensorflow.python.ops import variables
from tensorflow.python.platform import tf_logging as logging
from tensorflow.python.training import coordinator as thread_coordinator
from tensorflow.python.training import server_lib

_RPC_ERROR_FROM_WORKER = "GRPC error information from remote target /job:worker"
_RPC_ERROR_FROM_PS = "GRPC error information from remote target /job:ps"
_WORKER_PREEMPTION_THREAD_NAME = "WorkerPreemptionHandler"
_WORKER_THREAD_PREFIX = "WorkerClosureProcessingLoop"


class Model(object):

  def __init__(self, coordinator):
    self.cluster_coord = coordinator
    self.strategy = self.cluster_coord.strategy
    with self.cluster_coord.strategy.scope():
      self.build()

  def build(self):
    self.w = variables.Variable(
        initial_value=random_ops.random_uniform((10, 10)), dtype=dtypes.float32)
    self.iterations = variables.Variable(initial_value=0, dtype=dtypes.int32)
    # Allow external control to make the model run its train_fn in an infinite
    # loop. This allows us to reliably test worker preemption in the middle of
    # function execution.
    self.do_infinite_step = variables.Variable(False)

    def dataset_fn():
      data = random_ops.random_uniform((10, 10))
      dataset = dataset_ops.DatasetV2.from_tensors([data]).repeat()
      return dataset

    self.iterator = iter(
        self.cluster_coord.create_per_worker_dataset(dataset_fn))

  def _train_fn_internal(self, iterator):
    x = math_ops.matmul(array_ops.squeeze(next(iterator)), self.w)
    x = math_ops.matmul(random_ops.random_uniform((10, 10)), x)
    self.w.assign_add(x)

  @def_function.function
  def train_fn(self, iterator):
    self._train_fn_internal(iterator)
    while self.do_infinite_step:
      self._train_fn_internal(iterator)
    self.iterations.assign_add(1)

  def schedule_training_functions(self, num_steps):
    with self.strategy.scope():
      for _ in range(num_steps):
        self.cluster_coord.schedule(self.train_fn, args=(self.iterator,))

  def join_training_functions(self):
    self.do_infinite_step.assign(False)
    self.cluster_coord.join()


class BaseFaultToleranceTest(object):  # pylint: disable=missing-docstring

  def setUp(self, num_workers, num_ps):
    super(BaseFaultToleranceTest, self).setUp()

    # Set the environment variable to prevent hanging upon job failure and
    # restart. Note that it defaults to 'use_caller' at Google, but defaults
    # to False in OSS.
    os.environ["GRPC_FAIL_FAST"] = "use_caller"

    self._cluster = multi_worker_test_base.create_multi_process_cluster(
        num_workers=num_workers, num_ps=num_ps, rpc_layer="grpc")
    self._cluster_def = self._cluster.cluster_resolver.cluster_spec().as_dict()
    self._cluster_def["chief"] = [
        "localhost:%d" % multi_worker_test_base.pick_unused_port()
    ]
    cluster_resolver = SimpleClusterResolver(
        server_lib.ClusterSpec(self._cluster_def), rpc_layer="grpc")

    # The strategy's constructor would connect to the cluster.
    self.strategy = parameter_server_strategy_v2.ParameterServerStrategyV2(
        cluster_resolver)
    self.cluster_coord = cluster_coordinator.ClusterCoordinator(self.strategy)

    self.thread_coord = thread_coordinator.Coordinator(
        clean_stop_exception_types=[])
    self.num_workers = num_workers
    self.num_ps = num_ps

  def tearDown(self):
    super(BaseFaultToleranceTest, self).tearDown()
    self._cluster.stop()
    self._cluster = None

  def _restart(self, downtime_secs, job):
    """Kills `job` (index: 0) and restarts it after `downtime_secs`.

    Args:
      downtime_secs: secs before restarting the job.
      job: a string specifying the job to restart.
    """
    self._cluster.kill_task(job, 0)
    time.sleep(downtime_secs)
    self.assertFalse(context.check_alive("/job:%s/replica:0/task:0" % job))
    self._cluster.start_task(job, 0)
    while not context.check_alive("/job:%s/replica:0/task:0" % job):
      time.sleep(1)

  def _restart_in_thread(self, downtime_secs, restart_job):

    def _restart_fn():
      with self.thread_coord.stop_on_exception():
        self._restart(downtime_secs, restart_job)

    restart_thread = threading.Thread(target=_restart_fn)
    restart_thread.start()
    return restart_thread

  def _ensure_threads_closed(self):
    """Ensures worker and preemption threads are closed."""

    def _get_running_threads():
      """Returns a set of all running thread names."""
      running_threads = set()
      for thread in threading.enumerate():
        if thread.name is not None:
          running_threads.add(thread.name)
      return running_threads

    def _has_thread(prefix, running_threads):
      """Returns whether any 'running_threads' is prefixed with 'prefix'."""
      for thread in running_threads:
        if thread.startswith(prefix):
          return True
      return False

    # Worker and preemption threads should exist before releasing
    # ClusterCoordinator.
    running_threads = _get_running_threads()
    self.assertTrue(_has_thread(_WORKER_THREAD_PREFIX, running_threads))
    self.assertIn(_WORKER_PREEMPTION_THREAD_NAME, running_threads)

    # Wait for threads to close.
    self.cluster_coord = None
    self.strategy = None
    gc.collect()
    time.sleep(1)

    # Verify thread names.
    running_threads = _get_running_threads()
    self.assertNotIn(_WORKER_PREEMPTION_THREAD_NAME, running_threads)
    self.assertFalse(_has_thread(_WORKER_THREAD_PREFIX, running_threads))

  def _create_model_and_run_indefinitely(self):
    model = Model(self.cluster_coord)
    model.do_infinite_step.assign(True)
    model.schedule_training_functions(10)
    # Model does infinite training step, so at this moment, we expect to have
    # `self.num_workers` infinite closures inflight, and `10-self.num_workers`
    # closures in the queue.
    while (self.cluster_coord._cluster._closure_queue._inflight_closure_count <
           self.num_workers):
      time.sleep(0.1)
    return model

  def testClusterCoordinatorDestroyed(self):
    self._ensure_threads_closed()

  def testWorkerPreemptionBetweenFunctions(self):
    model = Model(self.cluster_coord)
    model.schedule_training_functions(2)
    model.join_training_functions()
    self.assertEqual(model.iterations.numpy(), 2)

    self._restart(downtime_secs=2, job="worker")

    model.schedule_training_functions(2)
    model.join_training_functions()
    self.assertEqual(model.iterations.numpy(), 4)

  def testWorkerPreemptionMidstFunction(self):
    model = Model(self.cluster_coord)
    model.do_infinite_step.assign(True)

    model.schedule_training_functions(4)
    # Model does infinite training step, so at this moment, we expect to have
    # `self.num_workers` infinite closures inflight, and `4-self.num_workers`
    # closures in the queue.
    while (self.cluster_coord._cluster._closure_queue._inflight_closure_count
           < self.num_workers):
      time.sleep(0.1)
    self.assertFalse(self.cluster_coord.done())
    self._restart(downtime_secs=2, job="worker")
    model.join_training_functions()
    self.assertGreaterEqual(model.iterations.numpy(), 4)

  def testOneWorkerPreemptionWithCancellation(self):

    @def_function.function
    def normal_function():
      x = random_ops.random_uniform((2, 10))
      y = random_ops.random_uniform((10, 2))
      return math_ops.reduce_mean(math_ops.matmul(x, y))

    @def_function.function
    def error_function():
      x = random_ops.random_uniform((2, 10))
      y = random_ops.random_uniform((10, 2))
      check_ops.assert_non_positive_v2(
          math_ops.reduce_sum(math_ops.matmul(x, y)))
      return x

    @def_function.function
    def long_function():
      x = random_ops.random_uniform((1000, 1000))
      for _ in math_ops.range(10000):
        a = random_ops.random_uniform((1000, 1000))
        b = random_ops.random_uniform((1000, 1000))
        x += math_ops.matmul(a, b)
      return x

    for _ in range(3):
      self.cluster_coord.schedule(normal_function)
    long_function_result = self.cluster_coord.schedule(long_function)
    self.cluster_coord.schedule(error_function)

    time.sleep(1)  # Let it run a couple steps.
    self._restart(1, "worker")

    with self.assertRaises(errors.InvalidArgumentError):
      self.cluster_coord.join()

    with self.assertRaises(errors.CancelledError):
      long_function_result.fetch()

    for _ in range(3):
      self.cluster_coord.schedule(normal_function)
    self.cluster_coord.join()

  def testHandleDatasetCreationFailure(self):
    model = Model(self.cluster_coord)

    restart_thread = self._restart_in_thread(5, "worker")

    model.schedule_training_functions(3)
    model.join_training_functions()

    self.thread_coord.join([restart_thread])
    self.assertGreaterEqual(model.iterations.numpy(), 3)

  def testWorkerPreemptionErrorType(self):

    @def_function.function
    def worker_train_fn():
      x = random_ops.random_uniform((2, 10))
      y = random_ops.random_uniform((10, 2))
      return math_ops.reduce_mean(math_ops.matmul(x, y))

    def run_fn():
      with self.thread_coord.stop_on_exception():
        with ops.device("/job:worker/replica:0/task:0"):
          for _ in range(3):
            for _ in range(3):
              worker_train_fn()
            time.sleep(5)

    run_thread = threading.Thread(target=run_fn)
    run_thread.start()
    time.sleep(1)  # Let it run a couple steps.
    self._restart(2, "worker")

    try:
      self.thread_coord.join([run_thread])
    except errors.UnavailableError as e:
      logging.info("Got exception %r, error message is %s", e, e)

      self.assertIn(_RPC_ERROR_FROM_WORKER, str(e))  # pylint: disable=g-assert-in-except
      self.assertNotIn(_RPC_ERROR_FROM_PS, str(e))

      self.assertTrue("failed to connect to all addresses" in str(e) or
                      "Unable to find a context_id" in str(e) or
                      "Socket closed" in str(e) or
                      "Connection reset by peer" in str(e) or
                      "Transport closed" in str(e))

  def testWorkerPreemptionErrorTypeWithPythonFunction(self):

    def worker_train_fn():
      x = random_ops.random_uniform((2, 10))
      y = random_ops.random_uniform((10, 2))
      return math_ops.reduce_mean(math_ops.matmul(x, y))

    def run_fn():
      with self.thread_coord.stop_on_exception():
        with ops.device("/job:worker/replica:0/task:0"):
          for _ in range(3):
            for _ in range(3):
              worker_train_fn()
            time.sleep(5)

    run_thread = threading.Thread(target=run_fn)
    run_thread.start()
    time.sleep(1)  # Let it run a couple steps.
    self._restart(2, "worker")

    try:
      self.thread_coord.join([run_thread])
    except errors.UnavailableError as e:
      logging.info("Got exception %r, error message is %s", e, e)

      self.assertIn(_RPC_ERROR_FROM_WORKER, str(e))  # pylint: disable=g-assert-in-except
      self.assertNotIn(_RPC_ERROR_FROM_PS, str(e))

      self.assertTrue("failed to connect to all addresses" in str(e) or
                      "Unable to find a context_id" in str(e) or
                      "Socket closed" in str(e) or
                      "Connection reset by peer" in str(e) or
                      "Transport closed" in str(e))

  def testPSPreemptionErrorType(self):

    with ops.device("/job:ps/replica:0/task:0"):
      v = variables.Variable(
          initial_value=random_ops.random_uniform((2, 10)),
          dtype=dtypes.float32)

    @def_function.function
    def worker_train_fn():
      y = random_ops.random_uniform((10, 2))
      return math_ops.reduce_mean(math_ops.matmul(v, y))

    def run_fn():
      with self.thread_coord.stop_on_exception():
        with ops.device("/job:worker/replica:0/task:0"):
          for _ in range(3):
            for _ in range(3):
              worker_train_fn()
            time.sleep(5)

    run_thread = threading.Thread(target=run_fn)
    run_thread.start()
    time.sleep(1)  # Let it run a couple steps.

    # Use a short restart delay to cover the case that RPC channel is reused
    self._restart(1, "ps")

    try:
      self.thread_coord.join([run_thread])
    except (errors.UnavailableError, errors.AbortedError) as e:
      logging.info("Got exception %r, error message is %s", e, e)
      self.assertIn(_RPC_ERROR_FROM_PS, str(e))  # pylint: disable=g-assert-in-except

      if isinstance(e, errors.UnavailableError):
        self.assertTrue("failed to connect to all addresses" in str(e) or
                        "Unable to find a context_id" in str(e) or
                        "Socket closed" in str(e) or
                        "Connection reset by peer" in str(e) or
                        "Transport closed" in str(e))

      if isinstance(e, errors.AbortedError):
        self.assertIn("RecvTensor expects a different device incarnation",
                      str(e))
      self._ensure_threads_closed()

  def testTwoWorkersPreempted(self):
    if self.num_workers < 2:
      self.skipTest("Worker number is less than 2.")
    model = self._create_model_and_run_indefinitely()

    self.assertFalse(self.cluster_coord.done())
    self._cluster.kill_task("worker", 0)
    self._cluster.kill_task("worker", 1)
    time.sleep(2)
    self.assertFalse(context.check_alive("/job:worker/replica:0/task:0"))
    self.assertFalse(context.check_alive("/job:worker/replica:0/task:1"))
    self._cluster.start_task("worker", 0)
    self._cluster.start_task("worker", 1)
    time.sleep(2)
    self.assertTrue(context.check_alive("/job:worker/replica:0/task:0"))
    self.assertTrue(context.check_alive("/job:worker/replica:0/task:1"))

    model.join_training_functions()
    self.assertGreaterEqual(model.iterations.numpy(), 10)

  def testWorkerContinuousFailure(self):
    model = self._create_model_and_run_indefinitely()

    self.assertFalse(self.cluster_coord.done())
    self._cluster.kill_task("worker", 0)
    time.sleep(2)
    self.assertFalse(context.check_alive("/job:worker/replica:0/task:0"))
    self._cluster.start_task("worker", 0)
    time.sleep(2)
    self.assertTrue(context.check_alive("/job:worker/replica:0/task:0"))
    self._cluster.kill_task("worker", 0)
    time.sleep(2)
    self.assertFalse(context.check_alive("/job:worker/replica:0/task:0"))
    self._cluster.start_task("worker", 0)
    time.sleep(2)
    self.assertTrue(context.check_alive("/job:worker/replica:0/task:0"))

    model.join_training_functions()
    self.assertGreaterEqual(model.iterations.numpy(), 10)

  def testNumpyFetchedAfterWorkerFailure(self):

    with self.strategy.scope():
      v = variables.Variable(initial_value=0, dtype=dtypes.int32)

    @def_function.function
    def worker_fn():
      return v + 1, v - 1

    remote_value = self.cluster_coord.schedule(worker_fn)
    # Attempt to fetch before killing worker task should succeed.
    self.assertEqual((1, -1), remote_value.fetch())
    self._cluster.kill_task("worker", 0)
    # So should attempt to fetch after killing worker task.
    self.assertEqual((1, -1), remote_value.fetch())

  def testClusterStateNotDisrupted(self):
    # This test has side effects and can disrupt other tests, even if the
    # resource created by it will not be used in following tests.
    # TODO(b/155209534): enable this test.
    # self.testPSPreemptionErrorType()

    self.thread_coord = thread_coordinator.Coordinator(
        clean_stop_exception_types=[])
    self.testWorkerPreemptionMidstFunction()

    self.thread_coord = thread_coordinator.Coordinator(
        clean_stop_exception_types=[])
    self.testWorkerPreemptionErrorType()

    # In previous tests, workers may fail after training is done. But the
    # following tests start with creating resources where failure is not
    # handled.
    # TODO(b/153888707): enable the following two tests.
    # self.testTwoWorkersPreempted()
    # self.testWorkerContinuousFailure()

  def testJoinRaisesUnavailableErrorAtPsFailure(self):
    self._create_model_and_run_indefinitely()
    self._cluster.kill_task("ps", 0)
    while self.cluster_coord._cluster._closure_queue._error is None:
      time.sleep(1)
    with self.assertRaises((errors.UnavailableError, errors.NotFoundError,
                            errors.FailedPreconditionError)):
      self.cluster_coord.join()

  def testScheduleRaisesUnavailableErrorAtPsFailure(self):
    self._create_model_and_run_indefinitely()
    self._cluster.kill_task("ps", 0)
    while self.cluster_coord._cluster._closure_queue._error is None:
      time.sleep(1)
    with self.assertRaises((errors.UnavailableError, errors.NotFoundError,
                            errors.FailedPreconditionError)):
      self.cluster_coord.schedule(def_function.function(lambda: None))

  def testWorkerExecutionAfterPsFailureRaisesExpectedError(self):
    model = self._create_model_and_run_indefinitely()
    for i in range(self.num_ps):
      self._cluster.kill_task("ps", i)
    while self.cluster_coord._cluster._closure_queue._error is None:
      time.sleep(1)

    @def_function.function
    def trivial_function():
      return model.iterations + 1

    for i in range(self.num_workers):
      try:
        with ops.device("/job:worker/replica:0/task:{}".format(i)):
          trivial_function()
      except Exception as e:  # pylint: disable=broad-except
        if cluster_coordinator._is_ps_failure(e):
          if i < self.num_workers - 1:
            continue
          return
      raise AssertionError("Executing a function after PS fails, should "
                           "result in a PS failure.")


class MultiWorkerFaultToleranceTest(BaseFaultToleranceTest, test.TestCase):
  """Multi worker fault tolerance tests.

  This covers the ordinary cases where multiple workers and PS are used.
  """

  def setUp(self):
    super(MultiWorkerFaultToleranceTest, self).setUp(2, 2)


class SingleWorkerFaultToleranceTest(BaseFaultToleranceTest, test.TestCase):
  """Single worker fault tolerance tests.

  This covers the cases that ensure training can continue in a single-worker
  cluster, even if the only worker can become unavailable at some point and
  recovered (if there are multiple workers, it is possible that the training
  succeeds with the workers that did not fail). Realistically single worker
  is very rarely used, but the tests are important to ensure the correct
  behaviors.
  """

  def setUp(self):
    super(SingleWorkerFaultToleranceTest, self).setUp(1, 1)


if __name__ == "__main__":
  v2_compat.enable_v2_behavior()
  multi_process_runner.test_main()
