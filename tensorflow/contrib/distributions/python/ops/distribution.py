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
"""Base classes for probability distributions."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import abc
import six

from tensorflow.python.framework import ops
from tensorflow.python.ops import math_ops


@six.add_metaclass(abc.ABCMeta)
class BaseDistribution(object):
  """Abstract base class for probability distributions.

  This class, along with `ContinuousDistribution` and `DiscreteDistribution`,
  defines the API for probability distributions.

  Users will never instantiate a `BaseDistribution`, but will instead
  instantiate subclasses of either `ContinuousDistribution` or
  `DiscreteDistribution`.

  Developers of new distributions should prefer to subclass
  `ContinuousDistribution` or `DiscreteDistribution`.

  ### API

  The key methods for probability distributions are defined here. The likelihood
  functions (`pdf`, `log_pdf`) and (`pmf`, `log_pmf`) are defined in
  `ContinuousDistribution` and `DiscreteDistribution`, respectively.

  To keep ops generated by the distribution tied together by name, subclasses
  should override `name` and use it to preprend names of ops in other methods
  (see `cdf` for an example).

  Subclasses that wish to support `cdf` and `log_cdf` can override `log_cdf`
  and use the base class's implementation for `cdf`.

  ### Broadcasting, batching, and shapes

  All distributions support batches of independent distributions of that type.
  The batch shape is determined by broadcasting together the parameters.

  The shape of arguments to `__init__`, `cdf`, `log_cdf`, and the likelihood
  functions defined in `ContinuousDistribution` and `DiscreteDistribution`
  reflect this broadcasting, as does the return value of `sample`.

  `sample_shape = (n,) + batch_shape + event_shape`, where `sample_shape` is the
  shape of the `Tensor` returned from `sample`, `n` is the number of samples,
  `batch_shape` defines how many independent distributions there are, and
  `event_shape` defines the shape of samples from each of those independent
  distributions. Samples are independent along the `batch_shape` dimensions,
  but not necessarily so along the `event_shape` dimensions (dependending on
  the particulars of the underlying distribution).

  Using the `Uniform` distribution as an example:

  ```python
  minval = 3.0
  maxval = [[4.0, 6.0],
            [10.0, 12.0]]

  # Broadcasting:
  # This instance represents 4 Uniform distributions. Each has a lower bound at
  # 3.0 as the `minval` parameter was broadcasted to match `maxval`'s shape.
  u = Uniform(minval, maxval)

  # `event_shape` is `TensorShape([])`.
  event_shape = u.get_event_shape()
  # `event_shape_t` is a `Tensor` which will evaluate to a scalar 1.
  event_shape_t = u.event_shape

  # Sampling returns a sample per distribution.  `samples` has shape
  # (5, 2, 2), which is (n,) + batch_shape + event_shape, where n=5,
  # batch_shape=(2, 2), and event_shape=().
  samples = u.sample(5)

  # The broadcasting holds across methods. Here we use `cdf` as an example. The
  # same holds for `log_cdf` and the likelihood functions.

  # `cum_prob` has shape (2, 2) as the `value` argument was broadcasted to the
  # shape of the `Uniform` instance.
  cum_prob_broadcast = u.cdf(4.0)

  # `cum_prob`'s shape is (2, 2), one per distribution. No broadcasting
  # occurred.
  cum_prob_per_dist = u.cdf([[4.0, 5.0],
                             [6.0, 7.0]])

  # INVALID as the `value` argument is not broadcastable to the distribution's
  # shape.
  cum_prob_invalid = u.cdf([4.0, 5.0, 6.0])
  ```
  """

  @abc.abstractproperty
  def name(self):
    """Name to prepend to all ops."""
    pass

  @abc.abstractproperty
  def dtype(self):
    """dtype of samples from this distribution."""
    pass

  @abc.abstractmethod
  def event_shape(self, name=None):
    """Shape of a sample from a single distribution as a 1-D int32 `Tensor`.

    Args:
      name: name to give to the op

    Returns:
      `Tensor` `event_shape`
    """
    pass

  @abc.abstractmethod
  def get_event_shape(self):
    """`TensorShape` available at graph construction time.

    Same meaning as `event_shape`. May be only partially defined.
    """
    pass

  @abc.abstractmethod
  def batch_shape(self, name=None):
    """Batch dimensions of this instance as a 1-D int32 `Tensor`.

    The product of the dimensions of the `batch_shape` is the number of
    independent distributions of this kind the instance represents.

    Args:
      name: name to give to the op

    Returns:
      `Tensor` `batch_shape`
    """
    pass

  @abc.abstractmethod
  def get_batch_shape(self):
    """`TensorShape` available at graph construction time.

    Same meaning as `batch_shape`. May be only partially defined.
    """
    pass

  def sample(self, n, seed=None, name=None):
    """Generate `n` samples.

    Args:
      n: scalar. Number of samples to draw from each distribution.
      seed: Python integer seed for RNG
      name: name to give to the op.

    Returns:
      samples: a `Tensor` of shape `(n,) + self.batch_shape + self.event_shape`
          with values of type `self.dtype`.
    """
    raise NotImplementedError("sample not implemented")

  def cdf(self, value, name="cdf"):
    """Cumulative distribution function."""
    value = ops.convert_to_tensor(value)
    with ops.op_scope([value], self.name):
      with ops.name_scope(name):
        return math_ops.exp(self.log_cdf(value))

  def log_cdf(self, value, name="log_cdf"):
    """Log CDF."""
    raise NotImplementedError("log_cdf is not implemented")

  def entropy(self, name=None):
    """Entropy of the distribution in nats."""
    raise NotImplementedError("entropy not implemented")

  @property
  def mean(self):
    raise NotImplementedError("mean not implemented")


class ContinuousDistribution(BaseDistribution):
  """Base class for continuous probability distributions.

  `ContinuousDistribution` defines the API for the likelihood functions `pdf`
  and `log_pdf` of continuous probability distributions, and a property
  `is_reparameterized` (returning `True` or `False`) which describes
  whether the samples of this distribution are calculated in a differentiable
  way from a non-parameterized distribution.  For example, the `Normal`
  distribution with parameters `mu` and `sigma` is reparameterized as

  ```Normal(mu, sigma) = sigma * Normal(0, 1) + mu```

  Subclasses must override `pdf` and `log_pdf` but one can call this base
  class's implementation.  They must also override the `is_reparameterized`
  property.

  See `BaseDistribution` for more information on the API for probability
  distributions.
  """

  @abc.abstractproperty
  def is_reparameterized(self):
    pass

  @abc.abstractmethod
  def pdf(self, value, name="pdf"):
    """Probability density function."""
    value = ops.convert_to_tensor(value)
    with ops.op_scope([value], self.name):
      with ops.name_scope(name):
        return math_ops.exp(self.log_pdf(value))

  @abc.abstractmethod
  def log_pdf(self, value, name="log_pdf"):
    """Log of the probability density function."""
    value = ops.convert_to_tensor(value)
    with ops.op_scope([value], self.name):
      with ops.name_scope(name):
        return math_ops.log(self.pdf(value))


class DiscreteDistribution(BaseDistribution):
  """Base class for discrete probability distributions.

  `DiscreteDistribution` defines the API for the likelihood functions `pmf` and
  `log_pmf` of discrete probability distributions.

  Subclasses must override both `pmf` and `log_pmf` but one can call this base
  class's implementation.

  See `BaseDistribution` for more information on the API for probability
  distributions.
  """

  @abc.abstractmethod
  def pmf(self, value, name="pmf"):
    """Probability mass function."""
    value = ops.convert_to_tensor(value)
    with ops.op_scope([value], self.name):
      with ops.name_scope(name):
        return math_ops.exp(self.log_pmf(value))

  @abc.abstractmethod
  def log_pmf(self, value, name="log_pmf"):
    """Log of the probability mass function."""
    value = ops.convert_to_tensor(value)
    with ops.op_scope([value], self.name):
      with ops.name_scope(name):
        return math_ops.log(self.pmf(value))
