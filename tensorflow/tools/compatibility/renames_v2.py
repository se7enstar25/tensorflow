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
# pylint: disable=line-too-long
"""List of renames to apply when converting from TF 1.0 to TF 2.0.

THIS FILE IS AUTOGENERATED: To update, please run:
  bazel build tensorflow/tools/compatibility/update:generate_v2_renames_map
  bazel-bin/tensorflow/tools/compatibility/update/generate_v2_renames_map
This file should be updated whenever endpoints are deprecated.
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

renames = {
    'tf.AUTO_REUSE': 'tf.compat.v1.AUTO_REUSE',
    'tf.COMPILER_VERSION': 'tf.version.COMPILER_VERSION',
    'tf.CXX11_ABI_FLAG': 'tf.sysconfig.CXX11_ABI_FLAG',
    'tf.FixedLengthRecordReader': 'tf.compat.v1.FixedLengthRecordReader',
    'tf.GIT_VERSION': 'tf.version.GIT_VERSION',
    'tf.GRAPH_DEF_VERSION': 'tf.version.GRAPH_DEF_VERSION',
    'tf.GRAPH_DEF_VERSION_MIN_CONSUMER': 'tf.version.GRAPH_DEF_VERSION_MIN_CONSUMER',
    'tf.GRAPH_DEF_VERSION_MIN_PRODUCER': 'tf.version.GRAPH_DEF_VERSION_MIN_PRODUCER',
    'tf.IdentityReader': 'tf.compat.v1.IdentityReader',
    'tf.InteractiveSession': 'tf.compat.v1.InteractiveSession',
    'tf.LMDBReader': 'tf.compat.v1.LMDBReader',
    'tf.MONOLITHIC_BUILD': 'tf.sysconfig.MONOLITHIC_BUILD',
    'tf.OpError': 'tf.errors.OpError',
    'tf.PaddingFIFOQueue': 'tf.io.PaddingFIFOQueue',
    'tf.Print': 'tf.compat.v1.Print',
    'tf.PriorityQueue': 'tf.io.PriorityQueue',
    'tf.QUANTIZED_DTYPES': 'tf.dtypes.QUANTIZED_DTYPES',
    'tf.QueueBase': 'tf.io.QueueBase',
    'tf.RandomShuffleQueue': 'tf.io.RandomShuffleQueue',
    'tf.ReaderBase': 'tf.compat.v1.ReaderBase',
    'tf.Session': 'tf.compat.v1.Session',
    'tf.SparseConditionalAccumulator': 'tf.sparse.SparseConditionalAccumulator',
    'tf.TFRecordReader': 'tf.compat.v1.TFRecordReader',
    'tf.TensorShape': 'tf.compat.v1.TensorShape',
    'tf.TextLineReader': 'tf.compat.v1.TextLineReader',
    'tf.VERSION': 'tf.version.VERSION',
    'tf.Variable': 'tf.compat.v1.Variable',
    'tf.VariableAggregation': 'tf.compat.v1.VariableAggregation',
    'tf.VariableScope': 'tf.compat.v1.VariableScope',
    'tf.WholeFileReader': 'tf.compat.v1.WholeFileReader',
    'tf.accumulate_n': 'tf.math.accumulate_n',
    'tf.add_to_collection': 'tf.compat.v1.add_to_collection',
    'tf.add_to_collections': 'tf.compat.v1.add_to_collections',
    'tf.all_variables': 'tf.compat.v1.all_variables',
    'tf.angle': 'tf.math.angle',
    'tf.assert_greater_equal': 'tf.debugging.assert_greater_equal',
    'tf.assert_integer': 'tf.debugging.assert_integer',
    'tf.assert_less_equal': 'tf.debugging.assert_less_equal',
    'tf.assert_near': 'tf.debugging.assert_near',
    'tf.assert_negative': 'tf.debugging.assert_negative',
    'tf.assert_non_negative': 'tf.debugging.assert_non_negative',
    'tf.assert_non_positive': 'tf.debugging.assert_non_positive',
    'tf.assert_none_equal': 'tf.debugging.assert_none_equal',
    'tf.assert_positive': 'tf.debugging.assert_positive',
    'tf.assert_proper_iterable': 'tf.debugging.assert_proper_iterable',
    'tf.assert_rank_at_least': 'tf.debugging.assert_rank_at_least',
    'tf.assert_rank_in': 'tf.debugging.assert_rank_in',
    'tf.assert_same_float_dtype': 'tf.debugging.assert_same_float_dtype',
    'tf.assert_scalar': 'tf.debugging.assert_scalar',
    'tf.assert_type': 'tf.debugging.assert_type',
    'tf.assert_variables_initialized': 'tf.compat.v1.assert_variables_initialized',
    'tf.assign': 'tf.compat.v1.assign',
    'tf.assign_add': 'tf.compat.v1.assign_add',
    'tf.assign_sub': 'tf.compat.v1.assign_sub',
    'tf.betainc': 'tf.math.betainc',
    'tf.bincount': 'tf.math.bincount',
    'tf.ceil': 'tf.math.ceil',
    'tf.check_numerics': 'tf.debugging.check_numerics',
    'tf.cholesky': 'tf.linalg.cholesky',
    'tf.cholesky_solve': 'tf.linalg.cholesky_solve',
    'tf.colocate_with': 'tf.compat.v1.colocate_with',
    'tf.confusion_matrix': 'tf.math.confusion_matrix',
    'tf.conj': 'tf.math.conj',
    'tf.count_up_to': 'tf.compat.v1.count_up_to',
    'tf.cross': 'tf.linalg.cross',
    'tf.cumprod': 'tf.math.cumprod',
    'tf.decode_base64': 'tf.io.decode_base64',
    'tf.decode_compressed': 'tf.io.decode_compressed',
    'tf.decode_csv': 'tf.io.decode_csv',
    'tf.decode_json_example': 'tf.io.decode_json_example',
    'tf.decode_raw': 'tf.io.decode_raw',
    'tf.delete_session_tensor': 'tf.compat.v1.delete_session_tensor',
    'tf.depth_to_space': 'tf.nn.depth_to_space',
    'tf.dequantize': 'tf.quantization.dequantize',
    'tf.deserialize_many_sparse': 'tf.io.deserialize_many_sparse',
    'tf.diag': 'tf.linalg.tensor_diag',
    'tf.diag_part': 'tf.linalg.tensor_diag_part',
    'tf.digamma': 'tf.math.digamma',
    'tf.dimension_at_index': 'tf.compat.v1.dimension_at_index',
    'tf.dimension_value': 'tf.compat.v1.dimension_value',
    'tf.disable_resource_variables': 'tf.compat.v1.disable_resource_variables',
    'tf.disable_v2_tensorshape': 'tf.compat.v1.disable_v2_tensorshape',
    'tf.distributions.Bernoulli': 'tf.compat.v1.distributions.Bernoulli',
    'tf.distributions.Beta': 'tf.compat.v1.distributions.Beta',
    'tf.distributions.Categorical': 'tf.compat.v1.distributions.Categorical',
    'tf.distributions.Dirichlet': 'tf.compat.v1.distributions.Dirichlet',
    'tf.distributions.DirichletMultinomial': 'tf.compat.v1.distributions.DirichletMultinomial',
    'tf.distributions.Distribution': 'tf.compat.v1.distributions.Distribution',
    'tf.distributions.Exponential': 'tf.compat.v1.distributions.Exponential',
    'tf.distributions.FULLY_REPARAMETERIZED': 'tf.compat.v1.distributions.FULLY_REPARAMETERIZED',
    'tf.distributions.Gamma': 'tf.compat.v1.distributions.Gamma',
    'tf.distributions.Laplace': 'tf.compat.v1.distributions.Laplace',
    'tf.distributions.Multinomial': 'tf.compat.v1.distributions.Multinomial',
    'tf.distributions.NOT_REPARAMETERIZED': 'tf.compat.v1.distributions.NOT_REPARAMETERIZED',
    'tf.distributions.Normal': 'tf.compat.v1.distributions.Normal',
    'tf.distributions.RegisterKL': 'tf.compat.v1.distributions.RegisterKL',
    'tf.distributions.ReparameterizationType': 'tf.compat.v1.distributions.ReparameterizationType',
    'tf.distributions.StudentT': 'tf.compat.v1.distributions.StudentT',
    'tf.distributions.Uniform': 'tf.compat.v1.distributions.Uniform',
    'tf.distributions.kl_divergence': 'tf.compat.v1.distributions.kl_divergence',
    'tf.enable_resource_variables': 'tf.compat.v1.enable_resource_variables',
    'tf.enable_v2_tensorshape': 'tf.compat.v1.enable_v2_tensorshape',
    'tf.encode_base64': 'tf.io.encode_base64',
    'tf.erf': 'tf.math.erf',
    'tf.erfc': 'tf.math.erfc',
    'tf.expm1': 'tf.math.expm1',
    'tf.extract_image_patches': 'tf.image.extract_image_patches',
    'tf.fake_quant_with_min_max_args': 'tf.quantization.fake_quant_with_min_max_args',
    'tf.fake_quant_with_min_max_args_gradient': 'tf.quantization.fake_quant_with_min_max_args_gradient',
    'tf.fake_quant_with_min_max_vars': 'tf.quantization.fake_quant_with_min_max_vars',
    'tf.fake_quant_with_min_max_vars_gradient': 'tf.quantization.fake_quant_with_min_max_vars_gradient',
    'tf.fake_quant_with_min_max_vars_per_channel': 'tf.quantization.fake_quant_with_min_max_vars_per_channel',
    'tf.fake_quant_with_min_max_vars_per_channel_gradient': 'tf.quantization.fake_quant_with_min_max_vars_per_channel_gradient',
    'tf.fft': 'tf.spectral.fft',
    'tf.floordiv': 'tf.math.floordiv',
    'tf.get_default_session': 'tf.compat.v1.get_default_session',
    'tf.get_local_variable': 'tf.compat.v1.get_local_variable',
    'tf.get_seed': 'tf.random.get_seed',
    'tf.get_session_handle': 'tf.compat.v1.get_session_handle',
    'tf.get_session_tensor': 'tf.compat.v1.get_session_tensor',
    'tf.get_variable': 'tf.compat.v1.get_variable',
    'tf.get_variable_scope': 'tf.compat.v1.get_variable_scope',
    'tf.global_norm': 'tf.linalg.global_norm',
    'tf.global_variables': 'tf.compat.v1.global_variables',
    'tf.global_variables_initializer': 'tf.compat.v1.global_variables_initializer',
    'tf.glorot_normal_initializer': 'tf.keras.initializers.glorot_normal',
    'tf.ifft': 'tf.spectral.ifft',
    'tf.igamma': 'tf.math.igamma',
    'tf.igammac': 'tf.math.igammac',
    'tf.imag': 'tf.math.imag',
    'tf.initialize_all_variables': 'tf.compat.v1.initialize_all_variables',
    'tf.initialize_local_variables': 'tf.compat.v1.initialize_local_variables',
    'tf.initialize_variables': 'tf.compat.v1.initialize_variables',
    'tf.initializers.global_variables': 'tf.compat.v1.initializers.global_variables',
    'tf.initializers.local_variables': 'tf.compat.v1.initializers.local_variables',
    'tf.initializers.variables': 'tf.compat.v1.initializers.variables',
    'tf.invert_permutation': 'tf.math.invert_permutation',
    'tf.is_finite': 'tf.debugging.is_finite',
    'tf.is_inf': 'tf.debugging.is_inf',
    'tf.is_nan': 'tf.debugging.is_nan',
    'tf.is_non_decreasing': 'tf.debugging.is_non_decreasing',
    'tf.is_numeric_tensor': 'tf.debugging.is_numeric_tensor',
    'tf.is_strictly_increasing': 'tf.debugging.is_strictly_increasing',
    'tf.is_variable_initialized': 'tf.compat.v1.is_variable_initialized',
    'tf.keras.backend.get_session': 'tf.compat.v1.keras.backend.get_session',
    'tf.layers.AveragePooling1D': 'tf.compat.v1.layers.AveragePooling1D',
    'tf.layers.AveragePooling2D': 'tf.compat.v1.layers.AveragePooling2D',
    'tf.layers.AveragePooling3D': 'tf.compat.v1.layers.AveragePooling3D',
    'tf.layers.BatchNormalization': 'tf.compat.v1.layers.BatchNormalization',
    'tf.layers.Conv1D': 'tf.compat.v1.layers.Conv1D',
    'tf.layers.Conv2D': 'tf.compat.v1.layers.Conv2D',
    'tf.layers.Conv2DTranspose': 'tf.compat.v1.layers.Conv2DTranspose',
    'tf.layers.Conv3D': 'tf.compat.v1.layers.Conv3D',
    'tf.layers.Conv3DTranspose': 'tf.compat.v1.layers.Conv3DTranspose',
    'tf.layers.Dense': 'tf.compat.v1.layers.Dense',
    'tf.layers.Dropout': 'tf.compat.v1.layers.Dropout',
    'tf.layers.Flatten': 'tf.compat.v1.layers.Flatten',
    'tf.layers.InputSpec': 'tf.keras.layers.InputSpec',
    'tf.layers.Layer': 'tf.compat.v1.layers.Layer',
    'tf.layers.MaxPooling1D': 'tf.compat.v1.layers.MaxPooling1D',
    'tf.layers.MaxPooling2D': 'tf.compat.v1.layers.MaxPooling2D',
    'tf.layers.MaxPooling3D': 'tf.compat.v1.layers.MaxPooling3D',
    'tf.layers.SeparableConv1D': 'tf.compat.v1.layers.SeparableConv1D',
    'tf.layers.SeparableConv2D': 'tf.compat.v1.layers.SeparableConv2D',
    'tf.layers.average_pooling1d': 'tf.compat.v1.layers.average_pooling1d',
    'tf.layers.average_pooling2d': 'tf.compat.v1.layers.average_pooling2d',
    'tf.layers.average_pooling3d': 'tf.compat.v1.layers.average_pooling3d',
    'tf.layers.batch_normalization': 'tf.compat.v1.layers.batch_normalization',
    'tf.layers.conv1d': 'tf.compat.v1.layers.conv1d',
    'tf.layers.conv2d': 'tf.compat.v1.layers.conv2d',
    'tf.layers.conv2d_transpose': 'tf.compat.v1.layers.conv2d_transpose',
    'tf.layers.conv3d': 'tf.compat.v1.layers.conv3d',
    'tf.layers.conv3d_transpose': 'tf.compat.v1.layers.conv3d_transpose',
    'tf.layers.dense': 'tf.compat.v1.layers.dense',
    'tf.layers.dropout': 'tf.compat.v1.layers.dropout',
    'tf.layers.experimental.keras_style_scope': 'tf.compat.v1.layers.experimental.keras_style_scope',
    'tf.layers.experimental.set_keras_style': 'tf.compat.v1.layers.experimental.set_keras_style',
    'tf.layers.flatten': 'tf.compat.v1.layers.flatten',
    'tf.layers.max_pooling1d': 'tf.compat.v1.layers.max_pooling1d',
    'tf.layers.max_pooling2d': 'tf.compat.v1.layers.max_pooling2d',
    'tf.layers.max_pooling3d': 'tf.compat.v1.layers.max_pooling3d',
    'tf.layers.separable_conv1d': 'tf.compat.v1.layers.separable_conv1d',
    'tf.layers.separable_conv2d': 'tf.compat.v1.layers.separable_conv2d',
    'tf.lbeta': 'tf.math.lbeta',
    'tf.lgamma': 'tf.math.lgamma',
    'tf.local_variables': 'tf.compat.v1.local_variables',
    'tf.local_variables_initializer': 'tf.compat.v1.local_variables_initializer',
    'tf.log_sigmoid': 'tf.math.log_sigmoid',
    'tf.logical_xor': 'tf.math.logical_xor',
    'tf.make_template': 'tf.compat.v1.make_template',
    'tf.manip.batch_to_space_nd': 'tf.batch_to_space_nd',
    'tf.manip.gather_nd': 'tf.gather_nd',
    'tf.manip.reshape': 'tf.reshape',
    'tf.manip.reverse': 'tf.reverse',
    'tf.manip.roll': 'tf.roll',
    'tf.manip.scatter_nd': 'tf.scatter_nd',
    'tf.manip.space_to_batch_nd': 'tf.space_to_batch_nd',
    'tf.manip.tile': 'tf.tile',
    'tf.matching_files': 'tf.io.matching_files',
    'tf.matrix_band_part': 'tf.linalg.band_part',
    'tf.matrix_determinant': 'tf.linalg.det',
    'tf.matrix_diag': 'tf.linalg.diag',
    'tf.matrix_diag_part': 'tf.linalg.diag_part',
    'tf.matrix_inverse': 'tf.linalg.inv',
    'tf.matrix_set_diag': 'tf.linalg.set_diag',
    'tf.matrix_solve': 'tf.linalg.solve',
    'tf.matrix_solve_ls': 'tf.linalg.lstsq',
    'tf.matrix_transpose': 'tf.linalg.transpose',
    'tf.matrix_triangular_solve': 'tf.linalg.triangular_solve',
    'tf.model_variables': 'tf.compat.v1.model_variables',
    'tf.moving_average_variables': 'tf.compat.v1.moving_average_variables',
    'tf.nn.ctc_beam_search_decoder': 'tf.compat.v1.nn.ctc_beam_search_decoder',
    'tf.nn.ctc_beam_search_decoder_v2': 'tf.nn.ctc_beam_search_decoder',
    'tf.nn.log_uniform_candidate_sampler': 'tf.random.log_uniform_candidate_sampler',
    'tf.nn.rnn_cell.BasicRNNCell': 'tf.compat.v1.nn.rnn_cell.BasicRNNCell',
    'tf.nn.softmax_cross_entropy_with_logits': 'tf.compat.v1.nn.softmax_cross_entropy_with_logits',
    'tf.nn.softmax_cross_entropy_with_logits_v2': 'tf.nn.softmax_cross_entropy_with_logits',
    'tf.nn.uniform_candidate_sampler': 'tf.random.uniform_candidate_sampler',
    'tf.orthogonal_initializer': 'tf.keras.initializers.Orthogonal',
    'tf.parse_tensor': 'tf.io.parse_tensor',
    'tf.placeholder': 'tf.compat.v1.placeholder',
    'tf.placeholder_with_default': 'tf.compat.v1.placeholder_with_default',
    'tf.polygamma': 'tf.math.polygamma',
    'tf.python_io.TFRecordCompressionType': 'tf.io.TFRecordCompressionType',
    'tf.python_io.TFRecordOptions': 'tf.io.TFRecordOptions',
    'tf.python_io.TFRecordWriter': 'tf.io.TFRecordWriter',
    'tf.python_io.tf_record_iterator': 'tf.io.tf_record_iterator',
    'tf.qr': 'tf.linalg.qr',
    'tf.quantize': 'tf.quantization.quantize',
    'tf.quantized_concat': 'tf.quantization.quantized_concat',
    'tf.random_gamma': 'tf.random.gamma',
    'tf.random_poisson': 'tf.random.poisson',
    'tf.read_file': 'tf.io.read_file',
    'tf.real': 'tf.math.real',
    'tf.reciprocal': 'tf.math.reciprocal',
    'tf.reduce_join': 'tf.strings.reduce_join',
    'tf.regex_replace': 'tf.strings.regex_replace',
    'tf.report_uninitialized_variables': 'tf.compat.v1.report_uninitialized_variables',
    'tf.reverse_v2': 'tf.reverse',
    'tf.rint': 'tf.math.rint',
    'tf.rsqrt': 'tf.math.rsqrt',
    'tf.saved_model.Builder': 'tf.compat.v1.saved_model.Builder',
    'tf.saved_model.TRAINING': 'tf.saved_model.TRANING',
    'tf.saved_model.build_tensor_info': 'tf.compat.v1.saved_model.build_tensor_info',
    'tf.saved_model.builder.SavedModelBuilder': 'tf.compat.v1.saved_model.builder.SavedModelBuilder',
    'tf.saved_model.constants.ASSETS_DIRECTORY': 'tf.saved_model.ASSETS_DIRECTORY',
    'tf.saved_model.constants.ASSETS_KEY': 'tf.saved_model.ASSETS_KEY',
    'tf.saved_model.constants.LEGACY_INIT_OP_KEY': 'tf.saved_model.LEGACY_INIT_OP_KEY',
    'tf.saved_model.constants.MAIN_OP_KEY': 'tf.saved_model.MAIN_OP_KEY',
    'tf.saved_model.constants.SAVED_MODEL_FILENAME_PB': 'tf.saved_model.SAVED_MODEL_FILENAME_PB',
    'tf.saved_model.constants.SAVED_MODEL_FILENAME_PBTXT': 'tf.saved_model.SAVED_MODEL_FILENAME_PBTXT',
    'tf.saved_model.constants.SAVED_MODEL_SCHEMA_VERSION': 'tf.saved_model.SAVED_MODEL_SCHEMA_VERSION',
    'tf.saved_model.constants.VARIABLES_DIRECTORY': 'tf.saved_model.VARIABLES_DIRECTORY',
    'tf.saved_model.constants.VARIABLES_FILENAME': 'tf.saved_model.VARIABLES_FILENAME',
    'tf.saved_model.experimental.save': 'tf.saved_model.save',
    'tf.saved_model.get_tensor_from_tensor_info': 'tf.compat.v1.saved_model.get_tensor_from_tensor_info',
    'tf.saved_model.load': 'tf.compat.v1.saved_model.load',
    'tf.saved_model.loader.load': 'tf.compat.v1.saved_model.loader.load',
    'tf.saved_model.loader.maybe_saved_model_directory': 'tf.saved_model.maybe_saved_model_directory',
    'tf.saved_model.main_op.main_op': 'tf.compat.v1.saved_model.main_op.main_op',
    'tf.saved_model.main_op.main_op_with_restore': 'tf.compat.v1.saved_model.main_op.main_op_with_restore',
    'tf.saved_model.main_op_with_restore': 'tf.compat.v1.saved_model.main_op_with_restore',
    'tf.saved_model.signature_constants.CLASSIFY_INPUTS': 'tf.saved_model.CLASSIFY_INPUTS',
    'tf.saved_model.signature_constants.CLASSIFY_METHOD_NAME': 'tf.saved_model.CLASSIFY_METHOD_NAME',
    'tf.saved_model.signature_constants.CLASSIFY_OUTPUT_CLASSES': 'tf.saved_model.CLASSIFY_OUTPUT_CLASSES',
    'tf.saved_model.signature_constants.CLASSIFY_OUTPUT_SCORES': 'tf.saved_model.CLASSIFY_OUTPUT_SCORES',
    'tf.saved_model.signature_constants.DEFAULT_SERVING_SIGNATURE_DEF_KEY': 'tf.saved_model.DEFAULT_SERVING_SIGNATURE_DEF_KEY',
    'tf.saved_model.signature_constants.PREDICT_INPUTS': 'tf.saved_model.PREDICT_INPUTS',
    'tf.saved_model.signature_constants.PREDICT_METHOD_NAME': 'tf.saved_model.PREDICT_METHOD_NAME',
    'tf.saved_model.signature_constants.PREDICT_OUTPUTS': 'tf.saved_model.PREDICT_OUTPUTS',
    'tf.saved_model.signature_constants.REGRESS_INPUTS': 'tf.saved_model.REGRESS_INPUTS',
    'tf.saved_model.signature_constants.REGRESS_METHOD_NAME': 'tf.saved_model.REGRESS_METHOD_NAME',
    'tf.saved_model.signature_constants.REGRESS_OUTPUTS': 'tf.saved_model.REGRESS_OUTPUTS',
    'tf.saved_model.signature_def_utils.build_signature_def': 'tf.saved_model.build_signature_def',
    'tf.saved_model.signature_def_utils.classification_signature_def': 'tf.saved_model.classification_signature_def',
    'tf.saved_model.signature_def_utils.is_valid_signature': 'tf.saved_model.is_valid_signature',
    'tf.saved_model.signature_def_utils.predict_signature_def': 'tf.saved_model.predict_signature_def',
    'tf.saved_model.signature_def_utils.regression_signature_def': 'tf.saved_model.regression_signature_def',
    'tf.saved_model.simple_save': 'tf.compat.v1.saved_model.simple_save',
    'tf.saved_model.tag_constants.GPU': 'tf.saved_model.GPU',
    'tf.saved_model.tag_constants.SERVING': 'tf.saved_model.SERVING',
    'tf.saved_model.tag_constants.TPU': 'tf.saved_model.TPU',
    'tf.saved_model.tag_constants.TRAINING': 'tf.saved_model.TRANING',
    'tf.saved_model.utils.build_tensor_info': 'tf.compat.v1.saved_model.utils.build_tensor_info',
    'tf.saved_model.utils.get_tensor_from_tensor_info': 'tf.compat.v1.saved_model.utils.get_tensor_from_tensor_info',
    'tf.scatter_add': 'tf.compat.v1.scatter_add',
    'tf.scatter_nd_add': 'tf.compat.v1.scatter_nd_add',
    'tf.scatter_nd_sub': 'tf.compat.v1.scatter_nd_sub',
    'tf.scatter_nd_update': 'tf.compat.v1.scatter_nd_update',
    'tf.scatter_sub': 'tf.compat.v1.scatter_sub',
    'tf.scatter_update': 'tf.compat.v1.scatter_update',
    'tf.segment_max': 'tf.math.segment_max',
    'tf.segment_mean': 'tf.math.segment_mean',
    'tf.segment_min': 'tf.math.segment_min',
    'tf.segment_prod': 'tf.math.segment_prod',
    'tf.segment_sum': 'tf.math.segment_sum',
    'tf.self_adjoint_eig': 'tf.linalg.eigh',
    'tf.self_adjoint_eigvals': 'tf.linalg.eigvalsh',
    'tf.serialize_many_sparse': 'tf.io.serialize_many_sparse',
    'tf.serialize_sparse': 'tf.io.serialize_sparse',
    'tf.space_to_batch': 'tf.nn.space_to_batch',
    'tf.space_to_depth': 'tf.nn.space_to_depth',
    'tf.sparse.placeholder': 'tf.compat.v1.sparse.placeholder',
    'tf.sparse_add': 'tf.sparse.add',
    'tf.sparse_fill_empty_rows': 'tf.sparse.fill_empty_rows',
    'tf.sparse_mask': 'tf.sparse.mask',
    'tf.sparse_maximum': 'tf.sparse.maximum',
    'tf.sparse_merge': 'tf.sparse.merge',
    'tf.sparse_minimum': 'tf.sparse.minimum',
    'tf.sparse_placeholder': 'tf.compat.v1.sparse_placeholder',
    'tf.sparse_reorder': 'tf.sparse.reorder',
    'tf.sparse_reset_shape': 'tf.sparse.reset_shape',
    'tf.sparse_reshape': 'tf.sparse.reshape',
    'tf.sparse_retain': 'tf.sparse.retain',
    'tf.sparse_segment_mean': 'tf.sparse.segment_mean',
    'tf.sparse_segment_sqrt_n': 'tf.sparse.segment_sqrt_n',
    'tf.sparse_segment_sum': 'tf.sparse.segment_sum',
    'tf.sparse_slice': 'tf.sparse.slice',
    'tf.sparse_softmax': 'tf.sparse.softmax',
    'tf.sparse_tensor_dense_matmul': 'tf.sparse.matmul',
    'tf.sparse_tensor_to_dense': 'tf.sparse.to_dense',
    'tf.sparse_to_indicator': 'tf.sparse.to_indicator',
    'tf.sparse_transpose': 'tf.sparse.transpose',
    'tf.squared_difference': 'tf.math.squared_difference',
    'tf.string_join': 'tf.strings.join',
    'tf.string_strip': 'tf.strings.strip',
    'tf.string_to_hash_bucket': 'tf.strings.to_hash_bucket',
    'tf.string_to_hash_bucket_fast': 'tf.strings.to_hash_bucket_fast',
    'tf.string_to_hash_bucket_strong': 'tf.strings.to_hash_bucket_strong',
    'tf.string_to_number': 'tf.strings.to_number',
    'tf.svd': 'tf.linalg.svd',
    'tf.test.compute_gradient_error': 'tf.compat.v1.test.compute_gradient_error',
    'tf.trace': 'tf.linalg.trace',
    'tf.train.MonitoredTrainingSession': 'tf.compat.v1.train.MonitoredTrainingSession',
    'tf.train.QueueRunner': 'tf.compat.v1.train.QueueRunner',
    'tf.train.Saver': 'tf.compat.v1.train.Saver',
    'tf.train.SaverDef': 'tf.compat.v1.train.SaverDef',
    'tf.train.SyncReplicasOptimizer': 'tf.compat.v1.train.SyncReplicasOptimizer',
    'tf.train.add_queue_runner': 'tf.compat.v1.train.add_queue_runner',
    'tf.train.basic_train_loop': 'tf.compat.v1.train.basic_train_loop',
    'tf.train.batch': 'tf.compat.v1.train.batch',
    'tf.train.batch_join': 'tf.compat.v1.train.batch_join',
    'tf.train.cosine_decay': 'tf.compat.v1.train.cosine_decay',
    'tf.train.cosine_decay_restarts': 'tf.compat.v1.train.cosine_decay_restarts',
    'tf.train.do_quantize_training_on_graphdef': 'tf.compat.v1.train.do_quantize_training_on_graphdef',
    'tf.train.exponential_decay': 'tf.compat.v1.train.exponential_decay',
    'tf.train.export_meta_graph': 'tf.compat.v1.train.export_meta_graph',
    'tf.train.import_meta_graph': 'tf.compat.v1.train.import_meta_graph',
    'tf.train.input_producer': 'tf.compat.v1.train.input_producer',
    'tf.train.inverse_time_decay': 'tf.compat.v1.train.inverse_time_decay',
    'tf.train.limit_epochs': 'tf.compat.v1.train.limit_epochs',
    'tf.train.linear_cosine_decay': 'tf.compat.v1.train.linear_cosine_decay',
    'tf.train.match_filenames_once': 'tf.io.match_filenames_once',
    'tf.train.maybe_batch': 'tf.compat.v1.train.maybe_batch',
    'tf.train.maybe_batch_join': 'tf.compat.v1.train.maybe_batch_join',
    'tf.train.maybe_shuffle_batch': 'tf.compat.v1.train.maybe_shuffle_batch',
    'tf.train.maybe_shuffle_batch_join': 'tf.compat.v1.train.maybe_shuffle_batch_join',
    'tf.train.natural_exp_decay': 'tf.compat.v1.train.natural_exp_decay',
    'tf.train.noisy_linear_cosine_decay': 'tf.compat.v1.train.noisy_linear_cosine_decay',
    'tf.train.piecewise_constant': 'tf.compat.v1.train.piecewise_constant',
    'tf.train.polynomial_decay': 'tf.compat.v1.train.polynomial_decay',
    'tf.train.queue_runner.QueueRunner': 'tf.compat.v1.train.queue_runner.QueueRunner',
    'tf.train.queue_runner.add_queue_runner': 'tf.compat.v1.train.queue_runner.add_queue_runner',
    'tf.train.queue_runner.start_queue_runners': 'tf.compat.v1.train.queue_runner.start_queue_runners',
    'tf.train.range_input_producer': 'tf.compat.v1.train.range_input_producer',
    'tf.train.shuffle_batch': 'tf.compat.v1.train.shuffle_batch',
    'tf.train.shuffle_batch_join': 'tf.compat.v1.train.shuffle_batch_join',
    'tf.train.slice_input_producer': 'tf.compat.v1.train.slice_input_producer',
    'tf.train.start_queue_runners': 'tf.compat.v1.train.start_queue_runners',
    'tf.train.string_input_producer': 'tf.compat.v1.train.string_input_producer',
    'tf.trainable_variables': 'tf.compat.v1.trainable_variables',
    'tf.uniform_unit_scaling_initializer': 'tf.initializers.uniform_unit_scaling',
    'tf.unsorted_segment_max': 'tf.math.unsorted_segment_max',
    'tf.unsorted_segment_mean': 'tf.math.unsorted_segment_mean',
    'tf.unsorted_segment_min': 'tf.math.unsorted_segment_min',
    'tf.unsorted_segment_prod': 'tf.math.unsorted_segment_prod',
    'tf.unsorted_segment_sqrt_n': 'tf.math.unsorted_segment_sqrt_n',
    'tf.unsorted_segment_sum': 'tf.math.unsorted_segment_sum',
    'tf.variable_creator_scope': 'tf.compat.v1.variable_creator_scope',
    'tf.variable_op_scope': 'tf.compat.v1.variable_op_scope',
    'tf.variable_scope': 'tf.compat.v1.variable_scope',
    'tf.variables_initializer': 'tf.compat.v1.variables_initializer',
    'tf.variance_scaling_initializer': 'tf.keras.initializers.VarianceScaling',
    'tf.verify_tensor_all_finite': 'tf.debugging.assert_all_finite',
    'tf.write_file': 'tf.io.write_file',
    'tf.zeta': 'tf.math.zeta'
}
