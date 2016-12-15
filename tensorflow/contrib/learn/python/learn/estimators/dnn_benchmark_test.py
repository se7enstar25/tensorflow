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

"""Regression test for DNNEstimator."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import functools
import numpy as np
import tensorflow as tf
from tensorflow.contrib.learn.python.learn.estimators import estimator_test_utils
from tensorflow.contrib.learn.python.learn.estimators import test_data


FLAGS = tf.flags.FLAGS


_METRIC_KEYS = {
    'accuracy',
    'auc',
    'accuracy/threshold_0.500000_mean',
    'loss',
    'precision/positive_threshold_0.500000_mean',
    'recall/positive_threshold_0.500000_mean',
}


class DNNClassifierBenchmark(tf.test.Benchmark):

  def _report_metrics(self, metrics):
    self.report_benchmark(
        iters=metrics['global_step'],
        extras={k: v for k, v in metrics.items() if k in _METRIC_KEYS})

  def _report_predictions(
      self, classifier, input_fn, iters, n_examples, n_classes,
      expected_probabilities=None, expected_classes=None):
    base_name = self._get_name()

    probabilities = classifier.predict_proba(
        input_fn=input_fn, as_iterable=False)
    self.report_benchmark(
        iters=iters, extras={
            'example%d_class%d_probability' % (i, j): probabilities[i][j]
            for j in range(n_classes) for i in range(n_examples)
        }, name='%s.inference.probabilities' % base_name)
    if expected_probabilities is not None:
      np.testing.assert_allclose(
          expected_probabilities, tuple(probabilities), atol=0.2)

    classes = classifier.predict(input_fn=input_fn, as_iterable=False)
    self.report_benchmark(
        iters=iters, extras={
            'example%d_class' % i: classes[i] for i in range(n_examples)
        }, name='%s.inference.classes' % base_name)
    if expected_classes is not None:
      np.testing.assert_array_equal(expected_classes, classes)

  def benchmarkLogisticMatrixData(self):
    classifier = tf.contrib.learn.DNNClassifier(
        feature_columns=(
            tf.contrib.layers.real_valued_column('feature', dimension=4),),
        hidden_units=(3, 3),
        config=tf.contrib.learn.RunConfig(tf_random_seed=1))
    input_fn = test_data.iris_input_logistic_fn
    steps = 400
    metrics = classifier.fit(input_fn=input_fn, steps=steps).evaluate(
        input_fn=input_fn, steps=1)
    estimator_test_utils.assert_in_range(
        steps, steps + 5, 'global_step', metrics)
    estimator_test_utils.assert_in_range(0.9, 1.0, 'accuracy', metrics)
    estimator_test_utils.assert_in_range(0.0, 0.3, 'loss', metrics)

    self._report_metrics(metrics)

  def benchmarkLogisticMatrixDataLabels1D(self):
    def _input_fn():
      iris = test_data.prepare_iris_data_for_logistic_regression()
      return {
          'feature': tf.constant(iris.data, dtype=tf.float32)
      }, tf.constant(iris.target, shape=(100,), dtype=tf.int32)

    classifier = tf.contrib.learn.DNNClassifier(
        feature_columns=(
            tf.contrib.layers.real_valued_column('feature', dimension=4),),
        hidden_units=(3, 3),
        config=tf.contrib.learn.RunConfig(tf_random_seed=1))
    steps = 1000
    metrics = classifier.fit(input_fn=_input_fn, steps=steps).evaluate(
        input_fn=_input_fn, steps=1)
    estimator_test_utils.assert_in_range(
        steps, steps + 5, 'global_step', metrics)
    estimator_test_utils.assert_in_range(0.9, 1.0, 'accuracy', metrics)

    self._report_metrics(metrics)

  def benchmarkLogisticNpMatrixData(self):
    classifier = tf.contrib.learn.DNNClassifier(
        feature_columns=(
            tf.contrib.layers.real_valued_column('', dimension=4),),
        hidden_units=(3, 3),
        config=tf.contrib.learn.RunConfig(tf_random_seed=1))
    iris = test_data.prepare_iris_data_for_logistic_regression()
    train_x = iris.data
    train_y = iris.target
    steps = 100
    metrics = classifier.fit(x=train_x, y=train_y, steps=steps).evaluate(
        x=train_x, y=train_y, steps=1)
    estimator_test_utils.assert_in_range(
        steps, steps + 5, 'global_step', metrics)
    estimator_test_utils.assert_in_range(0.8, 1.0, 'accuracy', metrics)

    self._report_metrics(metrics)

  def benchmarkLogisticTensorData(self):
    def _input_fn(num_epochs=None):
      features = {
          'age': tf.train.limit_epochs(
              tf.constant(((.8,), (0.2,), (.1,))), num_epochs=num_epochs),
          'language': tf.SparseTensor(
              values=tf.train.limit_epochs(
                  ('en', 'fr', 'zh'), num_epochs=num_epochs),
              indices=((0, 0), (0, 1), (2, 0)),
              shape=(3, 2))
      }
      return features, tf.constant(((1,), (0,), (0,)), dtype=tf.int32)

    lang_column = tf.contrib.layers.sparse_column_with_hash_bucket(
        'language', hash_bucket_size=20)
    classifier = tf.contrib.learn.DNNClassifier(
        feature_columns=(
            tf.contrib.layers.embedding_column(lang_column, dimension=1),
            tf.contrib.layers.real_valued_column('age')),
        hidden_units=(3, 3),
        config=tf.contrib.learn.RunConfig(tf_random_seed=1))
    steps = 100
    metrics = classifier.fit(input_fn=_input_fn, steps=steps).evaluate(
        input_fn=_input_fn, steps=1)
    estimator_test_utils.assert_in_range(
        steps, steps + 5, 'global_step', metrics)
    estimator_test_utils.assert_in_range(0.9, 1.0, 'accuracy', metrics)
    estimator_test_utils.assert_in_range(0.0, 0.3, 'loss', metrics)

    self._report_metrics(metrics)
    self._report_predictions(
        classifier=classifier,
        input_fn=functools.partial(_input_fn, num_epochs=1),
        iters=metrics['global_step'],
        n_examples=3,
        n_classes=2,
        expected_classes=(1, 0, 0))

  def benchmarkLogisticFloatLabel(self):
    def _input_fn(num_epochs=None):
      features = {
          'age': tf.train.limit_epochs(
              tf.constant(((50,), (20,), (10,))), num_epochs=num_epochs),
          'language': tf.SparseTensor(
              values=tf.train.limit_epochs(
                  ('en', 'fr', 'zh'), num_epochs=num_epochs),
              indices=((0, 0), (0, 1), (2, 0)),
              shape=(3, 2))
      }
      return features, tf.constant(((0.8,), (0.,), (0.2,)), dtype=tf.float32)

    lang_column = tf.contrib.layers.sparse_column_with_hash_bucket(
        'language', hash_bucket_size=20)
    n_classes = 2
    classifier = tf.contrib.learn.DNNClassifier(
        n_classes=n_classes,
        feature_columns=(
            tf.contrib.layers.embedding_column(lang_column, dimension=1),
            tf.contrib.layers.real_valued_column('age')),
        hidden_units=(3, 3),
        config=tf.contrib.learn.RunConfig(tf_random_seed=1))
    steps = 1000
    metrics = classifier.fit(input_fn=_input_fn, steps=steps).evaluate(
        input_fn=_input_fn, steps=1)
    estimator_test_utils.assert_in_range(
        steps, steps + 5, 'global_step', metrics)

    # Prediction probabilities mirror the labels column, which proves that the
    # classifier learns from float input.
    self._report_metrics(metrics)
    self._report_predictions(
        classifier=classifier,
        input_fn=functools.partial(_input_fn, num_epochs=1),
        iters=metrics['global_step'],
        n_examples=3,
        n_classes=n_classes,
        expected_probabilities=((0.2, 0.8), (1., 0.), (0.8, 0.2)),
        expected_classes=(1, 0, 0))

  def benchmarkMultiClassMatrixData(self):
    """Tests multi-class classification using matrix data as input."""
    classifier = tf.contrib.learn.DNNClassifier(
        n_classes=3,
        feature_columns=(
            tf.contrib.layers.real_valued_column('feature', dimension=4),),
        hidden_units=(3, 3),
        config=tf.contrib.learn.RunConfig(tf_random_seed=1))

    input_fn = test_data.iris_input_multiclass_fn
    steps = 500
    metrics = classifier.fit(input_fn=input_fn, steps=steps).evaluate(
        input_fn=input_fn, steps=1)
    estimator_test_utils.assert_in_range(
        steps, steps + 5, 'global_step', metrics)
    estimator_test_utils.assert_in_range(0.9, 1.0, 'accuracy', metrics)
    estimator_test_utils.assert_in_range(0.0, 0.4, 'loss', metrics)

    self._report_metrics(metrics)

if __name__ == '__main__':
  tf.test.main()
