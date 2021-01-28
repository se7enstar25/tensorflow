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
"""Benchmarks for the `MapVectorization` optimization."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function


import numpy as np

from tensorflow.core.example import example_pb2
from tensorflow.core.example import feature_pb2
from tensorflow.python.data.benchmarks import benchmark_base
from tensorflow.python.data.ops import dataset_ops
from tensorflow.python.data.util import nest
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import dtypes
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import parsing_ops


def _generate_csv_test_case():
  """Generates a `decode_csv()` test case."""

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
  """Generates a `parse_single_example()` test case."""

  def parse_example_factory():
    """Parse example factory."""

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


# TODO(rachelim): Add a benchmark for more expensive transformations, such as
# vgg_preprocessing.
class MapVectorizationBenchmark(benchmark_base.DatasetBenchmarkBase):
  """Benchmarks for the `MapVectorization` optimization."""

  def _run(self, dataset, num_elements, num_iters=100, name=None):

    wall_time = self.run_benchmark(
        dataset=dataset,
        num_elements=num_elements,
        iters=num_iters,
        warmup=True
    )
    self.report_benchmark(iters=num_iters, wall_time=wall_time, name=name)
    return wall_time

  def _compare(self, input_dataset, map_fn, batch_size, input_size, str_id):
    num_elements = int(np.sum([np.prod(x) for x in input_size]))
    name_template = "{}_batch_size_{}_input_element_size_{}_{}"

    unoptimized_dataset = input_dataset.map(map_fn).batch(batch_size)

    options = dataset_ops.Options()
    options.experimental_optimization.apply_default_optimizations = False
    unoptimized_dataset = unoptimized_dataset.with_options(options)

    options = dataset_ops.Options()
    options.experimental_optimization.map_vectorization.enabled = True
    optimized_dataset = unoptimized_dataset.with_options(options)

    unoptimized_time = self._run(
        dataset=unoptimized_dataset,
        num_elements=num_elements,
        name=name_template.format(str_id, batch_size, num_elements, "unoptimized"))
    optimized_time = self._run(
        dataset=optimized_dataset,
        num_elements=num_elements,
        name=name_template.format(str_id, batch_size, num_elements, "optimized"))

    print("Batch size: {}\n"
          "Input element size: {}\n"
          "Transformation: {}\n"
          "Speedup: {}\n".format(batch_size, input_size, str_id,
                                 (unoptimized_time / optimized_time)))

  # Known cheap functions
  def benchmark_identity(self):
    self._benchmark_helper(lambda *args: [array_ops.identity(x) for x in args],
                           "identity")

  def benchmark_add_const(self):
    self._benchmark_helper(lambda *args: [x + 1 for x in args], "add_const")

  def benchmark_return_const(self):
    self._benchmark_helper(lambda *args: [constant_op.constant(2)], "ret_const")

  def benchmark_select(self):
    self._benchmark_helper(lambda *args: args[0], "select")

  def benchmark_cast(self):
    self._benchmark_helper(
        lambda *args: [math_ops.cast(x, dtypes.float32) for x in args], "cast")

  def benchmark_reshape(self):
    self._benchmark_helper(
        lambda *args: [array_ops.reshape(x, (-1, 30)) for x in args], "reshape")

  def benchmark_decode_csv(self):
    csv_fn, csv_factory = _generate_csv_test_case()
    self._benchmark_helper(csv_fn, "decode_csv", lambda: [csv_factory()])

  def benchmark_parse_single_example(self):
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
          for shape in nest.flatten(
              dataset_ops.get_legacy_output_shapes(base_dataset))
      ]
      self._compare(base_dataset, map_fn, batch_size, input_size, str_id)


if __name__ == "__main__":
  benchmark_base.test.main()
