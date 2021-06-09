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
"""Utilities for tf.data options."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import collections

from absl import logging


def _internal_attr_name(name):
  return "_" + name


class OptionsBase(object):
  """Base class for representing a set of tf.data options.

  Attributes:
    _options: Stores the option values.
  """

  def __init__(self):
    # NOTE: Cannot use `self._options` here as we override `__setattr__`
    object.__setattr__(self, "_options", {})
    object.__setattr__(self, "_mutable", True)

  def __eq__(self, other):
    if not isinstance(other, self.__class__):
      return NotImplemented
    for name in set(self._options) | set(other._options):  # pylint: disable=protected-access
      if getattr(self, name) != getattr(other, name):
        return False
    return True

  def __ne__(self, other):
    if isinstance(other, self.__class__):
      return not self.__eq__(other)
    else:
      return NotImplemented

  def __setattr__(self, name, value):
    if not self._mutable:
      raise ValueError("Mutating `tf.data.Options()` returned by "
                       "`tf.data.Dataset.options()` has no effect. Use "
                       "`tf.data.Dataset.with_options(options)` to set or "
                       "update dataset options.")
    if hasattr(self, name):
      if name=="experimental_threading":
        logging.warning("options.experimental_threading is deprecated. "
                        "Use options.threading instead.")
        object.__setattr__(self, "threading", value)
      elif name=="threading":
        # ensure backward-compatibility until total deprecation of
        # `experimental_threading`
        object.__setattr__(self, "experimental_threading", value)
      object.__setattr__(self, name, value)
    else:
      raise AttributeError(
          "Cannot set the property %s on %s." % (name, type(self).__name__))

  def _set_mutable(self, mutable):
    """Change the mutability property to `mutable`."""
    object.__setattr__(self, "_mutable", mutable)

  def _to_proto(self):
    """Convert options to protocol buffer."""
    raise NotImplementedError("%s._to_proto()" % type(self).__name__)

  def _from_proto(self, pb):
    """Convert protocol buffer to options."""
    raise NotImplementedError("%s._from_proto()" % type(self).__name__)


# Creates a namedtuple with three keys for optimization graph rewrites settings.
def graph_rewrites():
  return collections.namedtuple("GraphRewrites",
                                ["enabled", "disabled", "default"])


def create_option(name, ty, docstring, default_factory=lambda: None):
  """Creates a type-checked property.

  Args:
    name: The name to use.
    ty: The type to use. The type of the property will be validated when it
      is set.
    docstring: The docstring to use.
    default_factory: A callable that takes no arguments and returns a default
      value to use if not set.

  Returns:
    A type-checked property.
  """

  def get_fn(option):
    # pylint: disable=protected-access
    if name not in option._options:
      option._options[name] = default_factory()
    return option._options.get(name)

  def set_fn(option, value):
    if not isinstance(value, ty):
      raise TypeError("Property \"%s\" must be of type %s, got: %r (type: %r)" %
                      (name, ty, value, type(value)))
    option._options[name] = value  # pylint: disable=protected-access

  return property(get_fn, set_fn, None, docstring)


def merge_options(*options_list):
  """Merges the given options, returning the result as a new options object.

  The input arguments are expected to have a matching type that derives from
  `tf.data.OptionsBase` (and thus each represent a set of options). The method
  outputs an object of the same type created by merging the sets of options
  represented by the input arguments.

  If an option is set to different values by different options objects, the
  result will match the setting of the options object that appears in the input
  list last.

  If an option is an instance of `tf.data.OptionsBase` itself, then this method
  is applied recursively to the set of options represented by this option.

  Args:
    *options_list: options to merge

  Raises:
    TypeError: if the input arguments are incompatible or not derived from
      `tf.data.OptionsBase`

  Returns:
    A new options object which is the result of merging the given options.
  """
  if len(options_list) < 1:
    raise ValueError("At least one options should be provided")
  result_type = type(options_list[0])

  for options in options_list:
    if not isinstance(options, result_type):
      raise TypeError("Incompatible options type: %r vs %r" % (type(options),
                                                               result_type))

  if not isinstance(options_list[0], OptionsBase):
    raise TypeError("The inputs should inherit from `OptionsBase`")

  default_options = result_type()
  result = result_type()
  for options in options_list:
    # Iterate over all set options and merge them into the result.
    for name in options._options:  # pylint: disable=protected-access
      this = getattr(result, name)
      that = getattr(options, name)
      default = getattr(default_options, name)
      if that == default:
        continue
      elif this == default:
        setattr(result, name, that)
      elif isinstance(this, OptionsBase):
        setattr(result, name, merge_options(this, that))
      elif this != that:
        logging.warning("Changing the value of option %s from %r to %r.", name,
                        this, that)
        setattr(result, name, that)
  return result
