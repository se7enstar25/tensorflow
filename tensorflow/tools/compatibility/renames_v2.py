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
    'tf.OpError': 'tf.errors.OpError',
    'tf.PaddingFIFOQueue': 'tf.io.PaddingFIFOQueue',
    'tf.PriorityQueue': 'tf.io.PriorityQueue',
    'tf.QueueBase': 'tf.io.QueueBase',
    'tf.RandomShuffleQueue': 'tf.io.RandomShuffleQueue',
    'tf.SparseConditionalAccumulator': 'tf.sparse.SparseConditionalAccumulator',
    'tf.accumulate_n': 'tf.math.accumulate_n',
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
    'tf.betainc': 'tf.math.betainc',
    'tf.bincount': 'tf.math.bincount',
    'tf.ceil': 'tf.math.ceil',
    'tf.check_numerics': 'tf.debugging.check_numerics',
    'tf.cholesky': 'tf.linalg.cholesky',
    'tf.cholesky_solve': 'tf.linalg.cholesky_solve',
    'tf.confusion_matrix': 'tf.math.confusion_matrix',
    'tf.conj': 'tf.math.conj',
    'tf.cross': 'tf.linalg.cross',
    'tf.cumprod': 'tf.math.cumprod',
    'tf.decode_base64': 'tf.io.decode_base64',
    'tf.decode_compressed': 'tf.io.decode_compressed',
    'tf.decode_csv': 'tf.io.decode_csv',
    'tf.decode_json_example': 'tf.io.decode_json_example',
    'tf.decode_raw': 'tf.io.decode_raw',
    'tf.depth_to_space': 'tf.nn.depth_to_space',
    'tf.dequantize': 'tf.quantization.dequantize',
    'tf.deserialize_many_sparse': 'tf.io.deserialize_many_sparse',
    'tf.diag': 'tf.linalg.tensor_diag',
    'tf.diag_part': 'tf.linalg.tensor_diag_part',
    'tf.digamma': 'tf.math.digamma',
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
    'tf.get_seed': 'tf.random.get_seed',
    'tf.global_norm': 'tf.linalg.global_norm',
    'tf.glorot_normal_initializer': 'tf.keras.initializers.glorot_normal',
    'tf.ifft': 'tf.spectral.ifft',
    'tf.igamma': 'tf.math.igamma',
    'tf.igammac': 'tf.math.igammac',
    'tf.imag': 'tf.math.imag',
    'tf.invert_permutation': 'tf.math.invert_permutation',
    'tf.is_finite': 'tf.debugging.is_finite',
    'tf.is_inf': 'tf.debugging.is_inf',
    'tf.is_nan': 'tf.debugging.is_nan',
    'tf.is_non_decreasing': 'tf.debugging.is_non_decreasing',
    'tf.is_numeric_tensor': 'tf.debugging.is_numeric_tensor',
    'tf.is_strictly_increasing': 'tf.debugging.is_strictly_increasing',
    'tf.lbeta': 'tf.math.lbeta',
    'tf.lgamma': 'tf.math.lgamma',
    'tf.log_sigmoid': 'tf.math.log_sigmoid',
    'tf.logical_xor': 'tf.math.logical_xor',
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
    'tf.nn.log_uniform_candidate_sampler': 'tf.random.log_uniform_candidate_sampler',
    'tf.nn.uniform_candidate_sampler': 'tf.random.uniform_candidate_sampler',
    'tf.orthogonal_initializer': 'tf.keras.initializers.Orthogonal',
    'tf.parse_tensor': 'tf.io.parse_tensor',
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
    'tf.reverse_v2': 'tf.reverse',
    'tf.rint': 'tf.math.rint',
    'tf.rsqrt': 'tf.math.rsqrt',
    'tf.saved_model.loader.maybe_saved_model_directory': 'tf.saved_model.maybe_saved_model_directory',
    'tf.saved_model.signature_def_utils.build_signature_def': 'tf.saved_model.build_signature_def',
    'tf.saved_model.signature_def_utils.classification_signature_def': 'tf.saved_model.classification_signature_def',
    'tf.saved_model.signature_def_utils.is_valid_signature': 'tf.saved_model.is_valid_signature',
    'tf.saved_model.signature_def_utils.predict_signature_def': 'tf.saved_model.predict_signature_def',
    'tf.saved_model.signature_def_utils.regression_signature_def': 'tf.saved_model.regression_signature_def',
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
    'tf.sparse_add': 'tf.sparse.add',
    'tf.sparse_fill_empty_rows': 'tf.sparse.fill_empty_rows',
    'tf.sparse_mask': 'tf.sparse.mask',
    'tf.sparse_maximum': 'tf.sparse.maximum',
    'tf.sparse_merge': 'tf.sparse.merge',
    'tf.sparse_minimum': 'tf.sparse.minimum',
    'tf.sparse_placeholder': 'tf.sparse.placeholder',
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
    'tf.trace': 'tf.linalg.trace',
    'tf.train.confusion_matrix': 'tf.math.confusion_matrix',
    'tf.train.match_filenames_once': 'tf.io.match_filenames_once',
    'tf.uniform_unit_scaling_initializer': 'tf.initializers.uniform_unit_scaling',
    'tf.unsorted_segment_max': 'tf.math.unsorted_segment_max',
    'tf.unsorted_segment_mean': 'tf.math.unsorted_segment_mean',
    'tf.unsorted_segment_min': 'tf.math.unsorted_segment_min',
    'tf.unsorted_segment_prod': 'tf.math.unsorted_segment_prod',
    'tf.unsorted_segment_sqrt_n': 'tf.math.unsorted_segment_sqrt_n',
    'tf.unsorted_segment_sum': 'tf.math.unsorted_segment_sum',
    'tf.variance_scaling_initializer': 'tf.keras.initializers.VarianceScaling',
    'tf.verify_tensor_all_finite': 'tf.debugging.assert_all_finite',
    'tf.write_file': 'tf.io.write_file',
    'tf.zeta': 'tf.math.zeta'
}
