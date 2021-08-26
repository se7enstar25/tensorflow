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
"""Wraps toco interface with python lazy loader."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

# We need to import pywrap_tensorflow prior to the toco wrapper.
# pylint: disable=invalid-import-order,g-bad-import-order
from tensorflow.python import pywrap_tensorflow  # pylint: disable=unused-import
from tensorflow.python import _pywrap_toco_api

# TODO(b/137402359): Remove lazy loading wrapper


def wrapped_toco_convert(model_flags_str, toco_flags_str, input_data_str,
                         debug_info_str, enable_mlir_converter):
  """Wraps TocoConvert with lazy loader."""
  return _pywrap_toco_api.TocoConvert(
      model_flags_str,
      toco_flags_str,
      input_data_str,
      False,  # extended_return
      debug_info_str,
      enable_mlir_converter)


def wrapped_experimental_mlir_quantize(input_data_str, disable_per_channel,
                                       fully_quantize, inference_type,
                                       input_data_type, output_data_type,
                                       enable_numeric_verify,
                                       enable_whole_model_verify,
                                       denylisted_ops, denylisted_nodes):
  """Wraps experimental mlir quantize model."""
  return _pywrap_toco_api.ExperimentalMlirQuantizeModel(
      input_data_str, disable_per_channel, fully_quantize, inference_type,
      input_data_type, output_data_type, enable_numeric_verify,
      enable_whole_model_verify, denylisted_ops, denylisted_nodes)


def wrapped_experimental_mlir_sparsify(input_data_str):
  """Wraps experimental mlir sparsify model."""
  return _pywrap_toco_api.ExperimentalMlirSparsifyModel(input_data_str)


def wrapped_register_custom_opdefs(custom_opdefs_list):
  """Wraps RegisterCustomOpdefs with lazy loader."""
  return _pywrap_toco_api.RegisterCustomOpdefs(custom_opdefs_list)


def wrapped_retrieve_collected_errors():
  """Wraps RetrieveCollectedErrors with lazy loader."""
  return _pywrap_toco_api.RetrieveCollectedErrors()


def wrapped_flat_buffer_file_to_mlir(model, input_is_filepath):
  """Wraps FlatBufferFileToMlir with lazy loader."""
  return _pywrap_toco_api.FlatBufferToMlir(model, input_is_filepath)
