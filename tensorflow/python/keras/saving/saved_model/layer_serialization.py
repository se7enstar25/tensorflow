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
"""Classes and functions implementing Layer SavedModel serialization."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.keras.mixed_precision.experimental import policy
from tensorflow.python.keras.saving.saved_model import base_serialization
from tensorflow.python.keras.saving.saved_model import constants
from tensorflow.python.keras.saving.saved_model import save_impl
from tensorflow.python.keras.saving.saved_model import serialized_attributes
from tensorflow.python.keras.utils.generic_utils import serialize_keras_object
from tensorflow.python.util import nest


class LayerSavedModelSaver(base_serialization.SavedModelSaver):
  """Implements Layer SavedModel serialization."""

  @property
  def object_identifier(self):
    return '_tf_keras_layer'

  @property
  def python_properties(self):
    # TODO(kathywu): Add python property validator
    return self._python_properties_internal()

  def _python_properties_internal(self):
    """Returns dictionary of all python properties."""
    # TODO(kathywu): Add support for metrics serialization.
    # TODO(kathywu): Synchronize with the keras spec (go/keras-json-spec) once
    # the python config serialization has caught up.
    metadata = dict(
        class_name=type(self.obj).__name__,
        name=self.obj.name,
        trainable=self.obj.trainable,
        expects_training_arg=self.obj._expects_training_arg,  # pylint: disable=protected-access
        dtype=policy.serialize(self.obj._dtype_policy),  # pylint: disable=protected-access
        batch_input_shape=getattr(self.obj, '_batch_input_shape', None))
    try:
      # Store the config dictionary, which is only used by the revived object
      # to return the original config when revived_obj.get_config() is called.
      # It is not important for recreating the revived object.
      metadata['config'] = self.obj.get_config()
    except NotImplementedError:
      # in the case of a subclassed model, the get_config() method will throw
      # a NotImplementedError.
      pass
    if self.obj.input_spec is not None:
      # Layer's input_spec has already been type-checked in the property setter.
      metadata['input_spec'] = nest.map_structure(
          lambda x: None if x is None else serialize_keras_object(x),
          self.obj.input_spec)
    if (self.obj.activity_regularizer is not None and
        hasattr(self.obj.activity_regularizer, 'get_config')):
      metadata['activity_regularizer'] = serialize_keras_object(
          self.obj.activity_regularizer)
    return metadata

  def objects_to_serialize(self, serialization_cache):
    return (self._get_serialized_attributes(
        serialization_cache).objects_to_serialize)

  def functions_to_serialize(self, serialization_cache):
    return (self._get_serialized_attributes(
        serialization_cache).functions_to_serialize)

  def _get_serialized_attributes(self, serialization_cache):
    """Generates or retrieves serialized attributes from cache."""
    keras_cache = serialization_cache.setdefault(constants.KERAS_CACHE_KEY, {})
    if self.obj in keras_cache:
      return keras_cache[self.obj]

    serialized_attr = keras_cache[self.obj] = (
        serialized_attributes.SerializedAttributes.new(self.obj))

    if save_impl.should_skip_serialization(self.obj):
      return serialized_attr

    object_dict, function_dict = self._get_serialized_attributes_internal(
        serialization_cache)

    serialized_attr.set_and_validate_objects(object_dict)
    serialized_attr.set_and_validate_functions(function_dict)
    return serialized_attr

  def _get_serialized_attributes_internal(self, serialization_cache):
    """Returns dictionary of serialized attributes."""
    objects = save_impl.wrap_layer_objects(self.obj, serialization_cache)
    functions = save_impl.wrap_layer_functions(self.obj, serialization_cache)
    # Attribute validator requires that the default save signature is added to
    # function dict, even if the value is None.
    functions['_default_save_signature'] = None
    return objects, functions


class InputLayerSavedModelSaver(base_serialization.SavedModelSaver):
  """InputLayer serialization."""

  @property
  def object_identifier(self):
    return '_tf_keras_input_layer'

  @property
  def python_properties(self):
    return dict(
        class_name=type(self.obj).__name__,
        name=self.obj.name,
        dtype=self.obj.dtype,
        sparse=self.obj.sparse,
        ragged=self.obj.ragged,
        batch_input_shape=self.obj._batch_input_shape,  # pylint: disable=protected-access
        config=self.obj.get_config())

  def objects_to_serialize(self, serialization_cache):
    return {}

  def functions_to_serialize(self, serialization_cache):
    return {}
