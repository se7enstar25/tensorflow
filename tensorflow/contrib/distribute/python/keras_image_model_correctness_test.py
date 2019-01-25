# Copyright 2019 The TensorFlow Authors. All Rights Reserved.
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
"""Correctness tests for tf.keras CNN models using DistributionStrategy."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np

from tensorflow.contrib.distribute.python import combinations
from tensorflow.contrib.distribute.python import keras_correctness_test_base
from tensorflow.python import keras
from tensorflow.python.eager import test
from tensorflow.python.training import gradient_descent


class DistributionStrategyCnnCorrectnessTest(
    keras_correctness_test_base.TestDistributionStrategyCorrectnessBase):

  def get_model(self, initial_weights=None, distribution=None):
    with keras_correctness_test_base.MaybeDistributionScope(distribution):
      image = keras.layers.Input(shape=(28, 28, 3), name='image')
      c1 = keras.layers.Conv2D(
          name='conv1', filters=16, kernel_size=(3, 3), strides=(4, 4))(
              image)
      if self.with_batch_norm:
        c1 = keras.layers.BatchNormalization(name='bn1')(c1)
      c1 = keras.layers.MaxPooling2D(pool_size=(2, 2))(c1)
      logits = keras.layers.Dense(
          10, activation='softmax', name='pred')(
              keras.layers.Flatten()(c1))
      model = keras.Model(inputs=[image], outputs=[logits])

      if initial_weights:
        model.set_weights(initial_weights)

      model.compile(
          optimizer=gradient_descent.GradientDescentOptimizer(
              learning_rate=0.1),
          loss='sparse_categorical_crossentropy',
          metrics=['sparse_categorical_accuracy'])

    return model

  def get_data(self,
               count=keras_correctness_test_base._GLOBAL_BATCH_SIZE
               * keras_correctness_test_base._EVAL_STEPS,
               shape=(28, 28, 3),
               num_classes=10):
    centers = np.random.randn(num_classes, *shape)

    features = []
    labels = []
    for _ in range(count):
      label = np.random.randint(0, num_classes, size=1)[0]
      offset = np.random.normal(loc=0, scale=0.1, size=np.prod(shape))
      offset = offset.reshape(shape)
      labels.append(label)
      features.append(centers[label] + offset)

    x_train = np.asarray(features, dtype=np.float32)
    y_train = np.asarray(labels, dtype=np.float32).reshape((count, 1))
    x_predict = x_train
    return x_train, y_train, x_predict

  @combinations.generate(keras_correctness_test_base.
                         all_strategy_and_input_config_combinations())
  def test_cnn_correctness(self, distribution, use_numpy, use_validation_data):
    self.run_correctness_test(distribution, use_numpy, use_validation_data)

  @combinations.generate(keras_correctness_test_base.
                         all_strategy_and_input_config_combinations())
  def test_cnn_with_batch_norm_correctness(self, distribution, use_numpy,
                                           use_validation_data):
    self.run_correctness_test(distribution, use_numpy, use_validation_data,
                              with_batch_norm=True)


if __name__ == '__main__':
  test.main()
