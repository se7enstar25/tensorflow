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
"""Benchmarks on convnet on MNIST dataset."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np

import tensorflow as tf

from tensorflow.python.keras.benchmarks import benchmark_util


class ConvMnistBenchmark(tf.test.Benchmark):
  """Benchmarks for convnet using `tf.test.Benchmark`."""
  # Required Arguments for measure_performance.
  #   x: Input data, it could be Numpy or load from tfds.
  #   y: Target data. If `x` is a dataset, generator instance,
  #      `y` should not be specified.
  #   loss: Loss function for model.
  #   optimizer: Optimizer for model.
  #   Other details can see in `measure_performance()` method of
  #   benchmark_util.

  def __init__(self):
    super(ConvMnistBenchmark, self).__init__()
    self.num_classes = 10
    self.input_shape = (28, 28, 1)
    (self.x_train, self.y_train), _ = tf.keras.datasets.mnist.load_data()
    self.x_train = self.x_train.astype('float32') / 255
    self.x_train = np.expand_dims(self.x_train, -1)
    self.y_train = tf.keras.utils.to_categorical(self.y_train, self.num_classes)
    self.epochs = 15

  def _build_model(self):
    """Model from https://keras.io/examples/vision/mnist_convnet/."""
    model = tf.keras.Sequential(
        [
            tf.keras.Input(shape=self.input_shape),
            tf.keras.layers.Conv2D(
                32, kernel_size=(3, 3), activation="relu"),
            tf.keras.layers.MaxPooling2D(
                pool_size=(2, 2)),
            tf.keras.layers.Conv2D(
                64, kernel_size=(3, 3), activation="relu"),
            tf.keras.layers.MaxPooling2D(
                pool_size=(2, 2)),
            tf.keras.layers.Flatten(),
            tf.keras.layers.Dropout(0.5),
            tf.keras.layers.Dense(
                self.num_classes, activation="softmax"),
        ]
    )
    return model

  def benchmark_conv_mnist_bs_128(self):
    """Measure performance with batch_size=128 and run_iters=2."""
    batch_size = 128
    run_iters = 2
    metrics, wall_time, extras = benchmark_util.measure_performance(
        self._build_model,
        x=self.x_train,
        y=self.y_train,
        batch_size=batch_size,
        run_iters=run_iters,
        epochs=self.epochs,
        optimizer="adam",
        loss='categorical_crossentropy',
        metrics=['accuracy'])

    self.report_benchmark(
        iters=run_iters,
        wall_time=wall_time,
        metrics=metrics,
        extras=extras)

  def benchmark_conv_mnist_bs_256(self):
    """Measure performance with batch_size=256 and run_iters=3."""
    batch_size = 256
    run_iters = 3
    metrics, wall_time, extras = benchmark_util.measure_performance(
        self._build_model,
        x=self.x_train,
        y=self.y_train,
        batch_size=batch_size,
        run_iters=run_iters,
        epochs=self.epochs,
        optimizer="adam",
        loss='categorical_crossentropy',
        metrics=['accuracy'])

    self.report_benchmark(
        iters=run_iters,
        wall_time=wall_time,
        metrics=metrics,
        extras=extras)

  def benchmark_conv_mnist_bs_512(self):
    """Measure performance with batch_size=512 and run_iters=4."""
    batch_size = 512
    run_iters = 3
    metrics, wall_time, extras = benchmark_util.measure_performance(
        self._build_model,
        x=self.x_train,
        y=self.y_train,
        batch_size=batch_size,
        run_iters=run_iters,
        epochs=self.epochs,
        optimizer="adam",
        loss='categorical_crossentropy',
        metrics=['accuracy'])

    self.report_benchmark(
        iters=run_iters,
        wall_time=wall_time,
        metrics=metrics,
        extras=extras)


if __name__ == '__main__':
  tf.test.main()
