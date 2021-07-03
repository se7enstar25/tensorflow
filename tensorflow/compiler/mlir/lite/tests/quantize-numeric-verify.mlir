// RUN: tf-opt %s -tfl-prepare-quantize -tfl-quantize -tfl-numeric-verify -tfl-log-if-failed | FileCheck --check-prefix=DEBUG %s
// RUN: tf-opt %s -tfl-prepare-quantize -tfl-quantize -tfl-numeric-verify -tfl-whole-model-verify -tfl-log-if-failed | FileCheck --check-prefix=MODEL-DEBUG %s

// DEBUG-LABEL: QuantizeConv2D
func @QuantizeConv2D(tensor<1x224x224x3x!quant.uniform<u8:f32, 7.812500e-03:128>>) -> tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>> {
^bb0(%arg0: tensor<1x224x224x3x!quant.uniform<u8:f32, 7.812500e-03:128>>):
  %cst = constant dense<-1.23697901> : tensor<32xf32>
  %2 = "tfl.dequantize"(%arg0) : (tensor<1x224x224x3x!quant.uniform<u8:f32, 7.812500e-03:128>>) -> tensor<1x224x224x3xf32>
  %w = constant dense<-1.0> : tensor<32x3x3x3xf32>
  %3 = "tfl.quantize"(%w) {qtype = tensor<32x3x3x3x!quant.uniform<u8<1:255>:f32, 0.1>>} : (tensor<32x3x3x3xf32>) -> tensor<32x3x3x3x!quant.uniform<u8<1:255>:f32, 0.1>>
  %4 = "tfl.dequantize"(%3) : (tensor<32x3x3x3x!quant.uniform<u8<1:255>:f32, 0.1>>) -> tensor<32x3x3x3xf32>
  %5 = "tfl.conv_2d"(%2, %4, %cst) {dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 2 : i32, stride_w = 2 : i32} : (tensor<1x224x224x3xf32>, tensor<32x3x3x3xf32>, tensor<32xf32>) -> tensor<1x112x112x32xf32>
  %6 = "tfl.quantize"(%5) {qtype = tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>>} : (tensor<1x112x112x32xf32>) -> tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>>
  return %6 : tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>>

// DEBUG-DAG: %[[wt:.*]] = constant dense<-1.000000e+00> : tensor<32x3x3x3xf32>
// DEBUG-DAG: %[[bias:.*]] = constant dense<-1.23697901> : tensor<32xf32>
// DEBUG: %[[act:.*]] = "tfl.dequantize"(%arg0) : (tensor<1x224x224x3x!quant.uniform<u8:f32, 7.812500e-03:128>>) -> tensor<1x224x224x3xf32>
// DEBUG: %[[f_conv:.*]] = "tfl.conv_2d"(%[[act]], %[[wt]], %[[bias]])
// DEBUG: %[[q_conv:.*]] = "tfl.conv_2d"
// DEBUG: "tfl.NumericVerify"(%[[q_conv]], %[[f_conv]]) {log_if_failed = true, tolerance = 5.000000e+00 : f32}
// DEBUG: return %[[q_conv]] : tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>>
}

// DEBUG-LABEL: QuantizeDepthwiseConv2D
func @QuantizeDepthwiseConv2D(tensor<1x224x224x3x!quant.uniform<u8:f32, 7.812500e-03:128>>) -> tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>> {
^bb0(%arg0: tensor<1x224x224x3x!quant.uniform<u8:f32, 7.812500e-03:128>>):
  %cst = constant dense<-1.23697901> : tensor<32xf32>
  %2 = "tfl.dequantize"(%arg0) : (tensor<1x224x224x3x!quant.uniform<u8:f32, 7.812500e-03:128>>) -> tensor<1x224x224x3xf32>
  %3 = "tfl.pseudo_qconst"() {qtype = tensor<32x3x3x3x!quant.uniform<u8<1:255>:f32, 0.021826678373682216:151>>, value = dense<-76> : tensor<32x3x3x3xi8>} : () -> tensor<32x3x3x3x!quant.uniform<u8<1:255>:f32, 0.021826678373682216:151>>
  %4 = "tfl.dequantize"(%3) : (tensor<32x3x3x3x!quant.uniform<u8<1:255>:f32, 0.021826678373682216:151>>) -> tensor<32x3x3x3xf32>
  %5 = "tfl.depthwise_conv_2d"(%2, %4, %cst) {depth_multiplier = 4 : i32, dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<1x224x224x3xf32>, tensor<32x3x3x3xf32>, tensor<32xf32>) -> tensor<1x112x112x32xf32>
  %6 = "tfl.quantize"(%5) {qtype = tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>>} : (tensor<1x112x112x32xf32>) -> tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>>
  return %6 : tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>>
}

// DEBUG-LABEL: QuantizeFullyConnected
func @QuantizeFullyConnected(tensor<1x224x224x3x!quant.uniform<u8:f32, 7.812500e-03:128>>) -> tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>> {
^bb0(%arg0: tensor<1x224x224x3x!quant.uniform<u8:f32, 7.812500e-03:128>>):
  %cst = constant dense<-1.23697901> : tensor<32xf32>
  %2 = "tfl.dequantize"(%arg0) : (tensor<1x224x224x3x!quant.uniform<u8:f32, 7.812500e-03:128>>) -> tensor<1x224x224x3xf32>
  %3 = "tfl.pseudo_qconst"() {qtype = tensor<32x12x!quant.uniform<u8<1:255>:f32, 0.021826678373682216:151>>, value = dense<-76> : tensor<32x12xi8>} : () -> tensor<32x12x!quant.uniform<u8<1:255>:f32, 0.021826678373682216:151>>
  %4 = "tfl.dequantize"(%3) : (tensor<32x12x!quant.uniform<u8<1:255>:f32, 0.021826678373682216:151>>) -> tensor<32x12xf32>
  %5 = "tfl.fully_connected"(%2, %4, %cst) {fused_activation_function = "NONE", keep_num_dims = false, weights_format = "DEFAULT"} : (tensor<1x224x224x3xf32>, tensor<32x12xf32>, tensor<32xf32>) -> tensor<1x112x112x32xf32>
  %6 = "tfl.quantize"(%5) {qtype = tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>>} : (tensor<1x112x112x32xf32>) -> tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>>
  return %6 : tensor<1x112x112x32x!quant.uniform<u8:f32, 0.023528476789885875>>
}

// DEBUG-LABEL: QuantizeSplit
func @QuantizeSplit(%arg: tensor<4x!quant.uniform<u8:f32, 1.0>>, %cst: tensor<i32>) -> (tensor<2x!quant.uniform<u8:f32, 1.0>>,tensor<2x!quant.uniform<u8:f32, 1.0>>) {
  %0 = "tfl.dequantize"(%arg) : (tensor<4x!quant.uniform<u8:f32, 1.0>>) -> tensor<4xf32>
  %1:2 = "tfl.split"(%cst, %0) {num_splits = 2 : i32} : (tensor<i32>, tensor<4xf32>) -> (tensor<2xf32>, tensor<2xf32>)
  %2 = "tfl.quantize"(%1#0) {qtype = tensor<2x!quant.uniform<u8:f32, 1.0>>} : (tensor<2xf32>) -> tensor<2x!quant.uniform<u8:f32, 1.0>>
  %3 = "tfl.quantize"(%1#1) {qtype = tensor<2x!quant.uniform<u8:f32, 1.0>>} : (tensor<2xf32>) -> tensor<2x!quant.uniform<u8:f32, 1.0>>
  return %2, %3 : tensor<2x!quant.uniform<u8:f32, 1.0>>, tensor<2x!quant.uniform<u8:f32, 1.0>>

// DEBUG: %[[f_split:.*]]:2 = "tfl.split"
// DEBUG: %[[q_split:.*]]:2 = "tfl.split"
// DEBUG: "tfl.NumericVerify"(%[[q_split]]#1, %[[f_split]]#1) {log_if_failed = true, tolerance = 5.000000e+00 : f32}
// DEBUG: "tfl.NumericVerify"(%[[q_split]]#0, %[[f_split]]#0) {log_if_failed = true, tolerance = 5.000000e+00 : f32}
}

// DEBUG-LABEL: NotQuantizePow
func @NotQuantizePow(%arg0: tensor<4x!quant.uniform<u8:f32, 1.0>>,
                     %arg1: tensor<4x!quant.uniform<u8:f32, 1.0>>) -> (tensor<4x!quant.uniform<u8:f32, 1.0>>) {
  %1 = "tfl.dequantize"(%arg0) : (tensor<4x!quant.uniform<u8:f32, 1.0>>) -> tensor<4xf32>
  %2 = "tfl.dequantize"(%arg1) : (tensor<4x!quant.uniform<u8:f32, 1.0>>) -> tensor<4xf32>
  %3 = "tfl.pow"(%1, %2) : (tensor<4xf32>, tensor<4xf32>) -> tensor<4xf32>
  %4 = "tfl.quantize"(%3) {qtype = tensor<4x!quant.uniform<u8:f32, 1.0>>} : (tensor<4xf32>) -> tensor<4x!quant.uniform<u8:f32, 1.0>>

  return %4 : tensor<4x!quant.uniform<u8:f32, 1.0>>
// DEBUG-NOT: "tfl.NumericVerify"
}

// DEBUG-LABEL: QuantizeCustomTfOp
func @QuantizeCustomTfOp(%arg0: tensor<128x128x!quant.uniform<u8:f32, 0.1:127>>,
    %arg1: tensor<1x!quant.uniform<u8:f32, 0.2:127>>, %arg2: tensor<1x!quant.uniform<u8:f32, 0.4:127>>,
    %arg3: tensor<1xi32>) -> (tensor<128x128x!quant.uniform<u8:f32, 0.2:125>>) {
  %0 = "tfl.dequantize"(%arg0) : (tensor<128x128x!quant.uniform<u8:f32, 0.1:127>>) -> tensor<128x128xf32>
  %1 = "tfl.dequantize"(%arg1) : (tensor<1x!quant.uniform<u8:f32, 0.2:127>>) -> tensor<1xf32>
  %2 = "tfl.dequantize"(%arg2) : (tensor<1x!quant.uniform<u8:f32, 0.4:127>>) -> tensor<1xf32>
  %3 = "tfl.custom_tf"(%0, %1, %2, %arg3) ( {
  ^bb0(%a1: tensor<128x128xf32>, %a2: tensor<1xf32>, %a3: tensor<1xf32>, %a4: tensor<1xi32>):  // no predecessors
    %4 = "tf.LayerNorm"(%a1, %a2, %a3, %a4) {_tfl_quant_trait = "fully_quantizable", device = ""} : (tensor<128x128xf32>, tensor<1xf32>, tensor<1xf32>, tensor<1xi32>) -> tensor<128x128xf32>
   "tfl.yield"(%4) : (tensor<128x128xf32>) -> ()
  }) {_tfl_quant_trait = "fully_quantizable", device = ""} : (tensor<128x128xf32>, tensor<1xf32>, tensor<1xf32>, tensor<1xi32>) -> tensor<128x128xf32>
  %4 = "tfl.quantize"(%3) {qtype = tensor<128x128x!quant.uniform<u8:f32, 0.2:125>>} : (tensor<128x128xf32>) -> tensor<128x128x!quant.uniform<u8:f32, 0.2:125>>
  return %4 : tensor<128x128x!quant.uniform<u8:f32, 0.2:125>>
}

// DEBUG-LABEL: NotQuantizeCustomTfOp
func @NotQuantizeCustomTfOp(%arg0: tensor<128x128x!quant.uniform<u8:f32, 0.1:127>>,
    %arg1: tensor<1x!quant.uniform<u8:f32, 0.2:127>>, %arg2: tensor<1x!quant.uniform<u8:f32, 0.4:127>>,
    %arg3: tensor<1xi32>) -> (tensor<128x128x!quant.uniform<u8:f32, 0.2:125>>) {
  %0 = "tfl.dequantize"(%arg0) : (tensor<128x128x!quant.uniform<u8:f32, 0.1:127>>) -> tensor<128x128xf32>
  %1 = "tfl.dequantize"(%arg1) : (tensor<1x!quant.uniform<u8:f32, 0.2:127>>) -> tensor<1xf32>
  %2 = "tfl.dequantize"(%arg2) : (tensor<1x!quant.uniform<u8:f32, 0.4:127>>) -> tensor<1xf32>
  %3 = "tfl.custom_tf"(%0, %1, %2, %arg3) ( {
  ^bb0(%a1: tensor<128x128xf32>, %a2: tensor<1xf32>, %a3: tensor<1xf32>, %a4: tensor<1xi32>):  // no predecessors
    %4 = "tf.LayerNorm"(%a1, %a2, %a3, %a4) {device = ""} : (tensor<128x128xf32>, tensor<1xf32>, tensor<1xf32>, tensor<1xi32>) -> tensor<128x128xf32>
   "tfl.yield"(%4) : (tensor<128x128xf32>) -> ()
  }) {device = ""} : (tensor<128x128xf32>, tensor<1xf32>, tensor<1xf32>, tensor<1xi32>) -> tensor<128x128xf32>
  %4 = "tfl.quantize"(%3) {qtype = tensor<128x128x!quant.uniform<u8:f32, 0.2:125>>} : (tensor<128x128xf32>) -> tensor<128x128x!quant.uniform<u8:f32, 0.2:125>>
  return %4 : tensor<128x128x!quant.uniform<u8:f32, 0.2:125>>
}

// DEBUG-LABEL: CheckNumericVerifyMultipleUsers
func @CheckNumericVerifyMultipleUsers(%arg0: tensor<1x5x5x3xf32>) -> tensor<1x5x5x3xf32> {
  %0 = "tfl.quantize"(%arg0) {qtype = tensor<1x5x5x3x!quant.uniform<i8:f32, 0.1>>, volatile} : (tensor<1x5x5x3xf32>) -> tensor<1x5x5x3x!quant.uniform<i8:f32, 0.1>>
  %1 = "tfl.dequantize"(%0) : (tensor<1x5x5x3x!quant.uniform<i8:f32, 0.1>>) -> tensor<1x5x5x3xf32>
  %2 = "tfl.average_pool_2d"(%1) {filter_height = 5 : i32, filter_width = 5 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 5 : i32, stride_w = 5 : i32} : (tensor<1x5x5x3xf32>) -> tensor<1x1x1x3xf32>
  %3 = "tfl.quantize"(%2) {qtype = tensor<1x1x1x3x!quant.uniform<i8:f32, 0.1>>, volatile} : (tensor<1x1x1x3xf32>) -> tensor<1x1x1x3x!quant.uniform<i8:f32, 0.1>>
  %4 = "tfl.dequantize"(%3) : (tensor<1x1x1x3x!quant.uniform<i8:f32, 0.1>>) -> tensor<1x1x1x3xf32>
  %5 = "tfl.add"(%1, %4) {fused_activation_function = "NONE"} : (tensor<1x5x5x3xf32>, tensor<1x1x1x3xf32>) -> tensor<1x5x5x3xf32>
  %6 = "tfl.quantize"(%5) {qtype = tensor<1x5x5x3x!quant.uniform<i8:f32, 0.1>>, volatile} : (tensor<1x5x5x3xf32>) -> tensor<1x5x5x3x!quant.uniform<i8:f32, 0.1>>
  %7 = "tfl.dequantize"(%6) : (tensor<1x5x5x3x!quant.uniform<i8:f32, 0.1>>) -> tensor<1x5x5x3xf32>
  return %7 : tensor<1x5x5x3xf32>
// DEBUG: %[[q:.*]] = "tfl.quantize"(%arg0)
// DEBUG: %[[dq:.*]] = "tfl.dequantize"(%[[q]])
// DEBUG: "tfl.average_pool_2d"(%[[dq]])
// DEBUG: "tfl.average_pool_2d"(%[[q]])
}

// MODEL-DEBUG-LABEL: CheckNumericVerifyWholeModel
func @CheckNumericVerifyWholeModel(%arg0: tensor<1x4x4x3xf32>) -> tensor<1x1x1x3xf32> {
  %0 = "tfl.quantize"(%arg0) {qtype = tensor<1x4x4x3x!quant.uniform<i8:f32, 0.1>>, volatile} : (tensor<1x4x4x3xf32>) -> tensor<1x4x4x3x!quant.uniform<i8:f32, 0.1>>
  %1 = "tfl.dequantize"(%0) : (tensor<1x4x4x3x!quant.uniform<i8:f32, 0.1>>) -> tensor<1x4x4x3xf32>
  %2 = "tfl.average_pool_2d"(%1) {filter_height = 2 : i32, filter_width = 2 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 2 : i32, stride_w = 2 : i32} : (tensor<1x4x4x3xf32>) -> tensor<1x2x2x3xf32>
  %3 = "tfl.quantize"(%2) {qtype = tensor<1x2x2x3x!quant.uniform<i8:f32, 0.1>>, volatile} : (tensor<1x2x2x3xf32>) -> tensor<1x2x2x3x!quant.uniform<i8:f32, 0.1>>
  %4 = "tfl.dequantize"(%3) : (tensor<1x2x2x3x!quant.uniform<i8:f32, 0.1>>) -> tensor<1x2x2x3xf32>
  %5 = "tfl.max_pool_2d"(%4) {filter_height = 2 : i32, filter_width = 2 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 1 : i32, stride_w = 1 : i32} : (tensor<1x2x2x3xf32>) -> tensor<1x1x1x3xf32>
  %6 = "tfl.quantize"(%5) {qtype = tensor<1x1x1x3x!quant.uniform<i8:f32, 0.1>>, volatile} : (tensor<1x1x1x3xf32>) -> tensor<1x1x1x3x!quant.uniform<i8:f32, 0.1>>
  %7 = "tfl.dequantize"(%6) : (tensor<1x1x1x3x!quant.uniform<i8:f32, 0.1>>) -> tensor<1x1x1x3xf32>
  return %7 : tensor<1x1x1x3xf32>
// MODEL-DEBUG: %[[q1:.*]] = "tfl.quantize"(%arg0)
// MODEL-DEBUG: %[[dq1:.*]] = "tfl.dequantize"(%[[q1]])
// MODEL-DEBUG: %[[f_out1:.*]] = "tfl.average_pool_2d"(%[[dq1]])
// MODEL-DEBUG: %[[q_out1:.*]] = "tfl.average_pool_2d"(%[[q1]])
// MODEL-DEBUG: "tfl.NumericVerify"(%[[q_out1]], %[[f_out1]])
// MODEL-DEBUG: %[[f_out2:.*]] = "tfl.max_pool_2d"(%[[f_out1]])
// MODEL-DEBUG: %[[q_out2:.*]] = "tfl.max_pool_2d"(%[[q_out1]])
// MODEL-DEBUG: "tfl.NumericVerify"(%[[q_out2]], %[[f_out2]])
}
