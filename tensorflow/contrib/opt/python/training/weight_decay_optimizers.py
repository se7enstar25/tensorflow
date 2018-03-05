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

"""Base class to make optimizers weight decay ready."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.framework import ops
from tensorflow.python.training import optimizer
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.training import adam, momentum
from tensorflow.python.util.tf_export import tf_export


def extend_with_decoupled_weight_decay(base_optimizer):
  """Factory function returning an optimizer class with decoupled weight decay.

  Returns an optimizer class. An instance of the returned class computes the
  update step of `base_optimizer` and additionally decays the weights.
  E.g., the class returned by
  `extend_with_decoupled_weight_decay(tf.train.AdamOptimizer)` is equivalent to
  `tf.contrib.opt.AdamWOptimizer`.

  The API of the new optimizer class slightly differs from the API of the
  base optimizer:
  - The first argument to the constructor is the weight decay rate.
  - `minimize` and `apply_gradients` accept the optional keyword argument
    `decay_var_list`, which specifies the variables that should be decayed.
    If `None`, all variables that are optimized are decayed.

  Usage example:
  ```python
  # MyAdamW is a new class
  MyAdamW = extend_with_decoupled_weight_decay(tf.train.AdamOptimizer)
  # Create a MyAdamW object
  optimizer = MyAdamW(weight_decay=0.001, learning_rate=0.001)
  sess.run(optimizer.minimize(loss, decay_variables=[var1, var2]))
  ```

  Args:
    base_optimizer: An optimizer class that inherits from tf.train.Optimizer.

  Returns:
    A new optimizer class that inherits from DecoupledWeightDecayExtension
    and base_optimizer.
  """
  class OptimizerWithDecoupledWeightDecay(DecoupledWeightDecayExtension,
                                          base_optimizer):
    """Base_optimizer with decoupled weight decay.

    This class computes the update step of `base_optimizer` and
    additionally decays the variable with the weight decay being decoupled from
    the optimization steps w.r.t. to the loss function, as described by
    Loshchilov & Hutter (https://arxiv.org/pdf/1711.05101.pdf).
    For SGD variants, this simplifies hyperparameter search since
    it decouples the settings of weight decay and learning rate.
    For adaptive gradient algorithms, it regularizes variables with large
    gradients more than L2 regularization would, which was shown to yield
    better training loss and generalization error in the paper above.
    """

    def __init__(self, weight_decay, *args, **kwargs):
      super(OptimizerWithDecoupledWeightDecay, self).__init__(
          weight_decay, *args, **kwargs)

  return OptimizerWithDecoupledWeightDecay


class DecoupledWeightDecayExtension(object):
  """This class allows to extend optimizers with decoupled weight decay.

  It implements the decoupled weight decay described by Loshchilov & Hutter
  (https://arxiv.org/pdf/1711.05101.pdf), in which the weight decay is
  decoupled from the optimization steps w.r.t. to the loss function.
  For SGD variants, this simplifies hyperparameter search since it decouples
  the settings of weight decay and learning rate.
  For adaptive gradient algorithms, it regularizes variables with large
  gradients more than L2 regularization would, which was shown to yield better
  training loss and generalization error in the paper above.

  This class alone is not an optimizer but rather extends existing
  optimizers with decoupled weight decay. We explicitly define the two examples
  used in the above paper (SGDW and AdamW), but in general this can extend
  any OptimizerX by using
  `extend_with_weight_decay(OptimizerX, weight_decay=weight_decay)`.
  In order for it to work, it must be the first class the Optimizer with
  weight decay inherits from, e.g.

  ```python
  class AdamWOptimizer(DecoupledWeightDecayExtension, adam.AdamOptimizer):
    def __init__(self, weight_decay, *args, **kwargs):
      super(AdamWOptimizer, self).__init__(weight_decay, *args, **kwargs).
  ```
  """

  def __init__(self, weight_decay, **kwargs):
    """Construct the extension class that adds weight decay to an optimizer.

    Args:
      weight_decay: A `Tensor` or a floating point value, the factor by which
        a variable is decayed in the update step.
      decay_var_list: Optional list or tuple or set of `Variable` objects to
        decay.
    """
    self._decay_var_list = None  # is set in minimize or apply_gradients
    self._weight_decay = weight_decay
    # The tensors are initialized in call to _prepare
    self._weight_decay_tensor = None
    super(DecoupledWeightDecayExtension, self).__init__(**kwargs)

  def minimize(self, loss, global_step=None, var_list=None,
               gate_gradients=optimizer.Optimizer.GATE_OP,
               aggregation_method=None, colocate_gradients_with_ops=False,
               name=None, grad_loss=None, decay_var_list=None):
    """Add operations to minimize `loss` by updating `var_list` with decay.

    This function is the same as Optimizer.minimize except that it allows to
    specify the variables that should be decayed using decay_var_list.
    If decay_var_list is None, all variables in var_list are decayed.

    For more information see the documentation of Optimizer.minimize.
    """
    self._decay_var_list = set(decay_var_list) if decay_var_list else False
    return super(DecoupledWeightDecayExtension, self).minimize(
        loss, global_step=global_step, var_list=var_list,
        gate_gradients=gate_gradients, aggregation_method=aggregation_method,
        colocate_gradients_with_ops=colocate_gradients_with_ops, name=name,
        grad_loss=grad_loss)

  def apply_gradients(self, grads_and_vars, global_step=None, name=None,
                      decay_var_list=None):
    """Apply gradients to variables and decay the variables.

    This function is the same as Optimizer.apply_gradients except that it
    allows to specify the variables that should be decayed using
    decay_var_list. If decay_var_list is None, all variables in var_list
    are decayed.

    For more information see the documentation of Optimizer.apply_gradients.
    """
    self._decay_var_list = set(decay_var_list) if decay_var_list else False
    return super(DecoupledWeightDecayExtension, self).apply_gradients(
        grads_and_vars, global_step=global_step, name=name)

  def _prepare(self):
    weight_decay = self._weight_decay
    if callable(weight_decay):
      weight_decay = weight_decay()
    self._weight_decay_tensor = ops.convert_to_tensor(
        weight_decay, name="weight_decay")
    # Call the optimizers _prepare function.
    super(DecoupledWeightDecayExtension, self)._prepare()

  def _decay_weights(self, var):
    if (not self._decay_var_list or
            (self._decay_var_list and var in self._decay_var_list)):
      return var.assign_sub(self._weight_decay * var, self._use_locking)
    return control_flow_ops.no_op()

  # Overwrite the apply functions the base optimizer calls. super().apply_x
  # resolves to the apply_x function of the child's BaseOptimizer.
  def _apply_dense(self, grad, var):
    with ops.control_dependencies([self._decay_weights(var)]):
      return super(DecoupledWeightDecayExtension, self)._apply_dense(grad, var)

  def _resource_apply_dense(self, grad, var):
    with ops.control_dependencies([self._decay_weights(var)]):
      return super(DecoupledWeightDecayExtension, self)._resource_apply_dense(
          grad, var)

  def _apply_sparse(self, grad, var):
    with ops.control_dependencies([self._decay_weights(var)]):
      return super(DecoupledWeightDecayExtension, self)._apply_sparse(
          grad, var)

  def _resource_apply_sparse(self, grad, var, indices):
    with ops.control_dependencies([self._decay_weights(var)]):
      return super(DecoupledWeightDecayExtension, self)._resource_apply_sparse(
          grad, var, indices)


@tf_export("contrib.opt.MomentumWOptimizer")
class MomentumWOptimizer(DecoupledWeightDecayExtension,
                         momentum.MomentumOptimizer):
  """Optimizer that implements the Momentum algorithm with weight_decay.

  This is an implementation of the SGDW optimizer described in "Fixing
  Weight Decay Regularization in Adam" by Loshchilov & Hutter
  (https://arxiv.org/abs/1711.05101)
  ([pdf])(https://arxiv.org/pdf/1711.05101.pdf).
  It computes the update step of `train.MomentumOptimizer` and additionally
  decays the variable. Note that this is different from adding
  L2 regularization on the variables to the loss. Decoupling the weight decay
  from other hyperparameters (in particular the learning rate) simplifies
  hyperparameter search.

  For further information see the documentation of the Momentum Optimizer.

  Note that this optimizer can also be instantiated as
  ```python
  extend_with_weight_decay(tf.train.MomentumOptimizer,
                           weight_decay=weight_decay)
  ```
  """

  def __init__(self, weight_decay, learning_rate, momentum,
               use_locking=False, name="MomentumW", use_nesterov=False):
    """Construct a new MomentumW optimizer.

    For further information see the documentation of the Momentum Optimizer.

    Args:
      weight_decay:  A `Tensor` or a floating point value.  The weight decay.
      learning_rate: A `Tensor` or a floating point value.  The learning rate.
      momentum: A `Tensor` or a floating point value.  The momentum.
      use_locking: If `True` use locks for update operations.
      name: Optional name prefix for the operations created when applying
        gradients.  Defaults to "Momentum".
      use_nesterov: If `True` use Nesterov Momentum.
        See [Sutskever et al., 2013](
        http://jmlr.org/proceedings/papers/v28/sutskever13.pdf).
        This implementation always computes gradients at the value of the
        variable(s) passed to the optimizer. Using Nesterov Momentum makes the
        variable(s) track the values called `theta_t + mu*v_t` in the paper.

    @compatibility(eager)
    When eager execution is enabled, learning_rate, weight_decay and momentum
    can each be a callable that takes no arguments and returns the actual value
    to use. This can be useful for changing these values across different
    invocations of optimizer functions.
    @end_compatibility
    """
    super(MomentumWOptimizer, self).__init__(
        weight_decay, learning_rate=learning_rate, momentum=momentum,
        use_locking=use_locking, name=name, use_nesterov=use_nesterov)


@tf_export("contrib.opt.AdamWOptimizer")
class AdamWOptimizer(DecoupledWeightDecayExtension, adam.AdamOptimizer):
  """Optimizer that implements the Adam algorithm with weight decay.

  This is an implementation of the AdamW optimizer described in "Fixing
  Weight Decay Regularization in Adam" by Loshchilov & Hutter
  (https://arxiv.org/abs/1711.05101)
  ([pdf])(https://arxiv.org/pdf/1711.05101.pdf).

  It computes the update step of `train.AdamOptimizer` and additionally decays
  the variable. Note that this is different from adding L2 regularization on
  the variables to the loss: it regularizes variables with large
  gradients more than L2 regularization would, which was shown to yield better
  training loss and generalization error in the paper above.

  For further information see the documentation of the Adam Optimizer.

  Note that this optimizer can also be instantiated as
  ```python
  extend_with_weight_decay(tf.train.AdamOptimizer, weight_decay=weight_decay)
  ```
  """

  def __init__(self, weight_decay, learning_rate=0.001, beta1=0.9, beta2=0.999,
               epsilon=1e-8, use_locking=False, name="AdamW"):
    """Construct a new AdamW optimizer.

    For further information see the documentation of the Adam Optimizer.

    Args:
      weight_decay:  A `Tensor` or a floating point value.  The weight decay.
      learning_rate: A Tensor or a floating point value.  The learning rate.
      beta1: A float value or a constant float tensor.
        The exponential decay rate for the 1st moment estimates.
      beta2: A float value or a constant float tensor.
        The exponential decay rate for the 2nd moment estimates.
      epsilon: A small constant for numerical stability. This epsilon is
        "epsilon hat" in the Kingma and Ba paper (in the formula just before
        Section 2.1), not the epsilon in Algorithm 1 of the paper.
      use_locking: If True use locks for update operations.
      name: Optional name for the operations created when applying gradients.
        Defaults to "Adam".
    """
    super(AdamWOptimizer, self).__init__(
        weight_decay, learning_rate=learning_rate, beta1=beta1, beta2=beta2,
        epsilon=epsilon, use_locking=use_locking, name=name)
