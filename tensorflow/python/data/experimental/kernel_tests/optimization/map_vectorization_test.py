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
"""Tests for the `MapVectorization` optimization."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import time

from absl.testing import parameterized
import numpy as np

from tensorflow.core.example import example_pb2
from tensorflow.core.example import feature_pb2
from tensorflow.python.client import session
from tensorflow.python.data.experimental.ops import optimization
from tensorflow.python.data.kernel_tests import test_base
from tensorflow.python.data.ops import dataset_ops
from tensorflow.python.data.util import nest
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import errors
from tensorflow.python.framework import ops
from tensorflow.python.framework import sparse_tensor
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import check_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import parsing_ops
from tensorflow.python.platform import test


def _generate_csv_test_case():

  def csv_factory():
    return dataset_ops.Dataset.from_tensor_slices(["1.0:2:a",
                                                   "2.4:5:c"]).repeat(5)

  def decode_csv_fn(x):
    return parsing_ops.decode_csv(
        x,
        record_defaults=[
            constant_op.constant([], dtypes.float32),
            constant_op.constant([], dtypes.int32),
            constant_op.constant([], dtypes.string)
        ],
        field_delim=":")

  return decode_csv_fn, csv_factory


def _generate_parse_single_example_test_case():

  def parse_example_factory():

    def _int64_feature(*values):
      return feature_pb2.Feature(int64_list=feature_pb2.Int64List(value=values))

    def _bytes_feature(*values):
      return feature_pb2.Feature(
          bytes_list=feature_pb2.BytesList(
              value=[v.encode("utf-8") for v in values]))

    return dataset_ops.Dataset.from_tensor_slices(
        constant_op.constant([
            example_pb2.Example(
                features=feature_pb2.Features(
                    feature={
                        "dense_int": _int64_feature(i),
                        "dense_str": _bytes_feature(str(i)),
                        "sparse_int": _int64_feature(i, i * 2, i * 4, i * 8),
                        "sparse_str": _bytes_feature(*["abc"] * i)
                    })).SerializeToString() for i in range(10)
        ]))

  def parse_single_example_fn(x):
    features = {
        "dense_int": parsing_ops.FixedLenFeature((), dtypes.int64, 0),
        "dense_str": parsing_ops.FixedLenFeature((), dtypes.string, ""),
        "sparse_int": parsing_ops.VarLenFeature(dtypes.int64),
        "sparse_str": parsing_ops.VarLenFeature(dtypes.string),
    }
    return parsing_ops.parse_single_example(x, features)

  return parse_single_example_fn, parse_example_factory


def _generate_optimization_test_cases():

  def base_dataset_factory():
    return dataset_ops.Dataset.from_tensors(np.random.rand(10, 3)).repeat(5)

  rand_val = np.random.rand(1, 1, 1, 1, 1, 1)

  csv_test_case = _generate_csv_test_case()
  parse_fn, parse_base = _generate_parse_single_example_test_case()

  def dense_output_only_parse_fn(x):
    # Since we haven't implemented a vectorizer for SerializeSparse, any
    # function with sparse outputs will only be naively vectorized.
    parse_result = parse_fn(x)
    return [
        y for y in parse_result if not isinstance(y, sparse_tensor.SparseTensor)
    ]

  # Misc test cases
  test_cases = [
      ("Basic", lambda x: (x, x + 1), base_dataset_factory),
      ("Const", lambda x: 2, base_dataset_factory),
      # Math ops exercise broadcasting capabilities
      ("Add", lambda x: x + rand_val, base_dataset_factory),
      ("Cast", lambda x: math_ops.cast(x, dtypes.float64),
       base_dataset_factory),
      ("Reshape", lambda x: array_ops.reshape(x, (-1, 30)),
       base_dataset_factory),
      ("Unpack", array_ops.unstack, base_dataset_factory),
      ("UnpackNegativeAxis", lambda x: array_ops.unstack(x, axis=-1),
       base_dataset_factory),
      # Parsing ops
      ("DecodeCSV", csv_test_case[0], csv_test_case[1]),
      ("ParseSingleExample", parse_fn, parse_base),
      ("ParseSingleExampleDenseOutputOnly", dense_output_only_parse_fn,
       parse_base),
  ]

  return [{
      "testcase_name":
          x[0] + "Parallel" if num_parallel_calls is not None else x[0],
      "map_fn":
          x[1],
      "base_dataset_factory":
          x[2],
      "num_parallel_calls":
          num_parallel_calls
  } for x in test_cases for num_parallel_calls in (None, 12)]


class MapVectorizationTest(test_base.DatasetTestBase, parameterized.TestCase):

  def _get_test_datasets(self,
                         base_dataset,
                         map_fn,
                         num_parallel_calls=None,
                         expect_optimized=True):
    """Given base dataset and map fn, creates test datasets.

    Returns a tuple of (unoptimized dataset, optimized dataset). The
    unoptimized dataset has the assertion that Batch follows Map. The optimized
    dataset has the assertion that Map follows Batch, and has the
    "map_vectorization" optimization applied.

    Args:
      base_dataset: Input dataset to map->batch
      map_fn: Map function to use
      num_parallel_calls: (Optional.) num_parallel_calls argument for map
      expect_optimized: (Optional.) Whether we expect the optimization to take
        place, in which case we will assert that Batch is followed by Map,
        otherwise Map followed by Batch. Defaults to True.

    Returns:
      Tuple of (unoptimized dataset, optimized dataset).
    """
    map_node_name = "Map" if num_parallel_calls is None else "ParallelMap"
    batch_size = 100

    def _make_dataset(node_names):
      return base_dataset.apply(optimization.assert_next(node_names)).map(
          map_fn, num_parallel_calls=num_parallel_calls).batch(batch_size)

    unoptimized = _make_dataset([map_node_name, "Batch"])
    optimized = _make_dataset(["Batch", map_node_name]
                              if expect_optimized else [map_node_name, "Batch"])
    options = dataset_ops.Options()
    options.experimental_map_vectorization = True
    optimized = optimized.with_options(options)
    return unoptimized, optimized

  @parameterized.named_parameters(_generate_optimization_test_cases())
  def testOptimization(self, map_fn, base_dataset_factory, num_parallel_calls):
    base_dataset = base_dataset_factory()
    unoptimized, optimized = self._get_test_datasets(base_dataset, map_fn,
                                                     num_parallel_calls)
    self.assertDatasetsEqual(unoptimized, optimized)

  def testOptimizationBadMapFn(self):
    # Test map functions that give an error
    def map_fn(x):
      # x has leading dimension 5, this will raise an error
      return array_ops.gather(x, 10)

    base_dataset = dataset_ops.Dataset.range(5).repeat(5).batch(
        5, drop_remainder=True)
    _, optimized = self._get_test_datasets(base_dataset, map_fn)
    nxt = optimized.make_one_shot_iterator().get_next()
    with self.assertRaisesRegexp(errors.InvalidArgumentError,
                                 r"indices = 10 is not in \[0, 5\)"):
      self.evaluate(nxt)

  def testOptimizationWithCapturedInputs(self):
    # Tests that vectorization works with captured inputs
    y = constant_op.constant(1, shape=(2,))
    z = constant_op.constant(2, shape=(2,))

    def map_fn(x):
      return x, y, z

    base_dataset = dataset_ops.Dataset.from_tensor_slices([[1, 2],
                                                           [3, 4]]).repeat(5)
    unoptimized, optimized = self._get_test_datasets(
        base_dataset, map_fn, expect_optimized=True)
    self.assertDatasetsEqual(optimized, unoptimized)

  def testOptimizationIgnoreStateful(self):

    def map_fn(x):
      with ops.control_dependencies([check_ops.assert_equal(x, 0)]):
        return array_ops.identity(x)

    base_dataset = dataset_ops.Dataset.from_tensor_slices([[1, 2],
                                                           [3, 4]]).repeat(5)
    unoptimized, optimized = self._get_test_datasets(
        base_dataset, map_fn, expect_optimized=False)
    self.assertDatasetsRaiseSameError(
        unoptimized, optimized, errors.InvalidArgumentError,
        [("OneShotIterator", "OneShotIterator_1", 1),
         ("IteratorGetNext", "IteratorGetNext_1", 1)])

  def testOptimizationIgnoreRagged(self):
    # Make sure we ignore inputs that might not be uniformly sized
    def map_fn(x):
      return array_ops.gather(x, 0)

    # output_shape = (?,)
    base_dataset = dataset_ops.Dataset.range(20).batch(3, drop_remainder=False)
    unoptimized, optimized = self._get_test_datasets(
        base_dataset, map_fn, expect_optimized=False)
    self.assertDatasetsEqual(unoptimized, optimized)

  def testOptimizationIgnoreRaggedMap(self):
    # Don't optimize when the output of the map fn shapes are unknown.
    def map_fn(x):
      return array_ops.tile(x, x)

    base_dataset = dataset_ops.Dataset.range(20).batch(1, drop_remainder=True)
    unoptimized, optimized = self._get_test_datasets(
        base_dataset, map_fn, expect_optimized=False)
    self.assertDatasetsRaiseSameError(
        unoptimized, optimized, errors.InvalidArgumentError,
        [("OneShotIterator", "OneShotIterator_1", 1),
         ("IteratorGetNext", "IteratorGetNext_1", 1)])


class MapVectorizationBenchmark(test.Benchmark):
  # TODO(rachelim): Add a benchmark for more expensive transformations, such as
  # vgg_preprocessing.

  def _run(self, x, num_iters=100, name=None):
    deltas = []
    with session.Session() as sess:
      for _ in range(5):
        # Warm up session...
        sess.run(x)
      for _ in range(num_iters):
        start = time.time()
        sess.run(x)
        end = time.time()
        deltas.append(end - start)
    median_time = np.median(deltas)
    self.report_benchmark(iters=num_iters, wall_time=median_time, name=name)
    return median_time

  def _compare(self, input_dataset, map_fn, batch_size, input_size, str_id):
    num_elems = int(np.sum([np.prod(x) for x in input_size]))
    name_template = "{}__batch_size_{}_input_element_size_{}_{}"
    unoptimized = input_dataset.map(map_fn).batch(batch_size)
    unoptimized_op = unoptimized.make_one_shot_iterator().get_next()

    optimized = input_dataset.map(map_fn).batch(batch_size)
    options = dataset_ops.Options()
    options.experimental_map_vectorization = True
    optimized = optimized.with_options(options)
    optimized_op = optimized.make_one_shot_iterator().get_next()

    unoptimized_time = self._run(
        unoptimized_op,
        name=name_template.format(str_id, batch_size, num_elems, "unoptimized"))
    optimized_time = self._run(
        optimized_op,
        name=name_template.format(str_id, batch_size, num_elems, "optimized"))

    print("Batch size: {}\n"
          "Input element size: {}\n"
          "Transformation: {}\n"
          "Speedup: {}\n".format(batch_size, input_size, str_id,
                                 (unoptimized_time / optimized_time)))

  # Known cheap functions
  def benchmarkIdentity(self):
    self._benchmark_helper(lambda *args: [array_ops.identity(x) for x in args],
                           "identity")

  def benchmarkAddConst(self):
    self._benchmark_helper(lambda *args: [x + 1 for x in args], "add_const")

  def benchmarkReturnConst(self):
    self._benchmark_helper(lambda *args: [constant_op.constant(2)], "ret_const")

  def benchmarkSelect(self):
    self._benchmark_helper(lambda *args: args[0], "select")

  def benchmarkCast(self):
    self._benchmark_helper(
        lambda *args: [math_ops.cast(x, dtypes.float64) for x in args], "cast")

  def benchmarkReshape(self):
    self._benchmark_helper(
        lambda *args: [array_ops.reshape(x, (-1, 30)) for x in args], "reshape")

  def benchmarkDecodeCSV(self):
    csv_fn, csv_factory = _generate_csv_test_case()
    self._benchmark_helper(csv_fn, "decode_csv", lambda: [csv_factory()])

  def benchmarkParseSingleExample(self):
    # NOTE: Since we haven't implemented a vectorizer for "SerializeSparse",
    # this function is only naively vectorized.
    parse_fn, parse_factory = _generate_parse_single_example_test_case()

    self._benchmark_helper(parse_fn, "parse_single_example",
                           lambda: [parse_factory()])

  def _default_dataset_factory(self):
    input_sizes = [(10, 10, 3), (10, 100, 300)]
    for sz in input_sizes:
      yield dataset_ops.Dataset.from_tensor_slices(np.random.rand(*sz))

  def _benchmark_helper(self, map_fn, str_id, base_dataset_factory=None):
    if base_dataset_factory is None:
      base_dataset_factory = self._default_dataset_factory

    batch_size = 1000
    for base_dataset in base_dataset_factory():
      base_dataset = base_dataset.repeat()
      input_size = [
          tuple(shape.as_list())
          for shape in nest.flatten(base_dataset.output_shapes)
      ]
      self._compare(base_dataset, map_fn, batch_size, input_size, str_id)


if __name__ == "__main__":
  test.main()
