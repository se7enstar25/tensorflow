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
"""Base testing class for strategies that require multiple nodes."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import contextlib
import copy
import threading
import numpy as np

from tensorflow.core.protobuf import config_pb2
from tensorflow.core.protobuf import rewriter_config_pb2
from tensorflow.python.client import session
from tensorflow.python.estimator import run_config
from tensorflow.python.platform import test
from tensorflow.python.framework import test_util


def create_in_process_cluster(num_workers, num_ps):
  """Create an in-process cluster that consists of only standard server."""
  # Leave some memory for cuda runtime.
  gpu_mem_frac = 0.7 / num_workers
  worker_config = config_pb2.ConfigProto()
  worker_config.gpu_options.per_process_gpu_memory_fraction = gpu_mem_frac

  ps_config = config_pb2.ConfigProto()
  ps_config.device_count['GPU'] = 0

  # Create in-process servers. Once an in-process tensorflow server is created,
  # there is no way to terminate it. So we create one cluster per test process.
  # We could've started the server in another process, we could then kill that
  # process to terminate the server. The reasons why we don't want multiple
  # processes are
  # 1) it is more difficult to manage these processes;
  # 2) there is something global in CUDA such that if we initialize CUDA in the
  # parent process, the child process cannot initialize it again and thus cannot
  # use GPUs (https://stackoverflow.com/questions/22950047).
  return test_util.create_local_cluster(
      num_workers,
      num_ps=num_ps,
      worker_config=worker_config,
      ps_config=ps_config,
      protocol='grpc')


class MultiWorkerTestBase(test.TestCase):
  """Base class for testing multi node strategy and dataset."""

  @classmethod
  def setUpClass(cls):
    """Create a local cluster with 2 workers."""
    cls._workers, cls._ps = create_in_process_cluster(num_workers=2, num_ps=0)

  def setUp(self):
    # We only cache the session in one test because another test may have a
    # different session config or master target.
    self._thread_local = threading.local()
    self._thread_local.cached_session = None
    self._result = 0
    self._lock = threading.Lock()

  @contextlib.contextmanager
  def test_session(self, graph=None, config=None, target=None):
    """Create a test session with master target set to the testing cluster.

    This overrides the base class' method, removes arguments that are not needed
    by the multi-node case and creates a test session that connects to the local
    testing cluster.

    Args:
      graph: Optional graph to use during the returned session.
      config: An optional config_pb2.ConfigProto to use to configure the
        session.

    Yields:
      A Session object that should be used as a context manager to surround
      the graph building and execution code in a test case.
    """
    if self.id().endswith('.test_session'):
      self.skipTest('Not a test.')

    if config is None:
      config = config_pb2.ConfigProto(allow_soft_placement=True)
    else:
      config = copy.deepcopy(config)
    # Don't perform optimizations for tests so we don't inadvertently run
    # gpu ops on cpu
    config.graph_options.optimizer_options.opt_level = -1
    config.graph_options.rewrite_options.constant_folding = (
        rewriter_config_pb2.RewriterConfig.OFF)

    if graph is None:
      if getattr(self._thread_local, 'cached_session', None) is None:
        self._thread_local.cached_session = session.Session(
            graph=None, config=config, target=target or self._workers[0].target)
      sess = self._thread_local.cached_session
      with sess.graph.as_default(), sess.as_default():
        yield sess
    else:
      with session.Session(
          graph=graph, config=config, target=target or
          self._workers[0].target) as sess:
        yield sess

  def _run_client(self, client_fn, task_type, task_id, num_gpus, *args,
                  **kwargs):
    result = client_fn(task_type, task_id, num_gpus, *args, **kwargs)
    if np.all(result):
      with self._lock:
        self._result += 1

  def _run_between_graph_clients(self, client_fn, cluster_spec, num_gpus, *args,
                                 **kwargs):
    """Runs several clients for between-graph replication.

    Args:
      client_fn: a function that needs to accept `task_type`, `task_id`,
        `num_gpus` and returns True if it succeeds.
      cluster_spec: a dict specifying jobs in a cluster.
      num_gpus: number of GPUs per worker.
      *args: will be passed to `client_fn`.
      **kwargs: will be passed to `client_fn`.
    """
    threads = []
    for task_type in [run_config.TaskType.CHIEF, run_config.TaskType.WORKER]:
      for task_id in range(len(cluster_spec.get(task_type, []))):
        t = threading.Thread(
            target=self._run_client,
            args=(client_fn, task_type, task_id, num_gpus) + args,
            kwargs=kwargs)
        t.start()
        threads.append(t)
    for t in threads:
      t.join()
    self.assertEqual(self._result, len(threads))
