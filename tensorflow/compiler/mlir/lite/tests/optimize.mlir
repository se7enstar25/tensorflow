// RUN: tf-opt %s -tfl-optimize | FileCheck %s

// CHECK-LABEL: fusedConv2dRelu
func @fusedConv2dRelu(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<16x3x3x3xf32>, %arg2: tensor<16xf32>) -> tensor<256x30x30x16xf32> {
  %0 = "tfl.conv_2d"(%arg0, %arg1, %arg2) {dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  %1 = "tfl.relu"(%0) : (tensor<256x30x30x16xf32>) -> tensor<256x30x30x16xf32>
  return %1 : tensor<256x30x30x16xf32>

  // CHECK: %0 = "tfl.conv_2d"(%arg0, %arg1, %arg2) {dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "RELU", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  // CHECK: return %0
}

// CHECK-LABEL: fusedDepthwiseConv2dRelu6
func @fusedDepthwiseConv2dRelu6(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<16x3x3x3xf32>, %arg2: tensor<16xf32>) -> tensor<256x30x30x16xf32> {
  %0 = "tfl.depthwise_conv_2d"(%arg0, %arg1, %arg2) {depth_multiplier = 4 : i32, dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  %1 = "tfl.relu6"(%0) : (tensor<256x30x30x16xf32>) -> tensor<256x30x30x16xf32>
  return %1 : tensor<256x30x30x16xf32>

  // CHECK: %0 = "tfl.depthwise_conv_2d"(%arg0, %arg1, %arg2) {depth_multiplier = 4 : i32, dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "RELU6", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  // CHECK: return %0
}

// CHECK-LABEL: fusedConv2dTanh
func @fusedConv2dTanh(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<16x3x3x3xf32>, %arg2: tensor<16xf32>) -> tensor<256x30x30x16xf32> {
  %0 = "tfl.conv_2d"(%arg0, %arg1, %arg2) {dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  %1 = "tfl.tanh"(%0) : (tensor<256x30x30x16xf32>) -> tensor<256x30x30x16xf32>
  return %1 : tensor<256x30x30x16xf32>

  // CHECK: %0 = "tfl.conv_2d"(%arg0, %arg1, %arg2) {dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "TANH", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  // CHECK: return %0
}

// CHECK-LABEL: fuseAddIntoConv2d
func @fuseAddIntoConv2d(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<16x3x3x3xf32>) -> tensor<256x30x30x16xf32> {
  %cst = constant dense<1.5> : tensor<16xf32>
  %cst_0 = constant dense<[1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0]> : tensor<16xf32>
  %0 = "tfl.conv_2d"(%arg0, %arg1, %cst_0) {dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  %1 = "tfl.add"(%0, %cst) {fused_activation_function = "NONE"} : (tensor<256x30x30x16xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  return %1 : tensor<256x30x30x16xf32>

  // CHECK: %cst = constant dense<[2.500000e+00, 3.500000e+00, 4.500000e+00, 5.500000e+00, 6.500000e+00, 7.500000e+00, 8.500000e+00, 9.500000e+00, 1.050000e+01, 1.150000e+01, 1.250000e+01, 1.350000e+01, 1.450000e+01, 1.550000e+01, 1.650000e+01, 1.750000e+01]> : tensor<16xf32>
  // CHECK: %0 = "tfl.conv_2d"(%arg0, %arg1, %cst)
}

// CHECK-LABEL: @fuseAddIntoDepthwiseConv2d
func @fuseAddIntoDepthwiseConv2d(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<16x3x3x3xf32>) -> tensor<256x30x30x16xf32> {
  %cst = constant dense<[1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0]> : tensor<16xf32>
  %cst_0 = constant dense<1.5> : tensor<16xf32>
  %0 = "tfl.depthwise_conv_2d"(%arg0, %arg1, %cst_0) {depth_multiplier = 4 : i32, dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  %1 = "tfl.add"(%0, %cst) {fused_activation_function = "NONE"} : (tensor<256x30x30x16xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  return %1 : tensor<256x30x30x16xf32>

  // CHECK: %cst = constant dense<[2.500000e+00, 3.500000e+00, 4.500000e+00, 5.500000e+00, 6.500000e+00, 7.500000e+00, 8.500000e+00, 9.500000e+00, 1.050000e+01, 1.150000e+01, 1.250000e+01, 1.350000e+01, 1.450000e+01, 1.550000e+01, 1.650000e+01, 1.750000e+01]> : tensor<16xf32>
  // CHECK: %0 = "tfl.depthwise_conv_2d"(%arg0, %arg1, %cst)
}

// CHECK-LABEL: fuseAddWithRelu6IntoConv2d
func @fuseAddWithRelu6IntoConv2d(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<16x3x3x3xf32>) -> tensor<256x30x30x16xf32> {
  %cst = constant dense<1.5> : tensor<16xf32>
  %cst_0 = constant dense<[1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0]> : tensor<16xf32>
  %0 = "tfl.conv_2d"(%arg0, %arg1, %cst_0) {dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  %1 = "tfl.add"(%0, %cst) {fused_activation_function = "RELU6"} : (tensor<256x30x30x16xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  return %1 : tensor<256x30x30x16xf32>

  // CHECK: %cst = constant dense<[2.500000e+00, 3.500000e+00, 4.500000e+00, 5.500000e+00, 6.500000e+00, 7.500000e+00, 8.500000e+00, 9.500000e+00, 1.050000e+01, 1.150000e+01, 1.250000e+01, 1.350000e+01, 1.450000e+01, 1.550000e+01, 1.650000e+01, 1.750000e+01]> : tensor<16xf32>
  // CHECK: %0 = "tfl.conv_2d"(%arg0, %arg1, %cst)
  // CHECK-SAME: fused_activation_function = "RELU6"
}

// CHECK-LABEL: @fuseAddWithRelu6IntoDepthwiseConv2d
func @fuseAddWithRelu6IntoDepthwiseConv2d(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<16x3x3x3xf32>) -> tensor<256x30x30x16xf32> {
  %cst = constant dense<[1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0]> : tensor<16xf32>
  %cst_0 = constant dense<1.5> : tensor<16xf32>
  %0 = "tfl.depthwise_conv_2d"(%arg0, %arg1, %cst_0) {depth_multiplier = 4 : i32, dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  %1 = "tfl.add"(%0, %cst) {fused_activation_function = "RELU6"} : (tensor<256x30x30x16xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  return %1 : tensor<256x30x30x16xf32>

  // CHECK: %cst = constant dense<[2.500000e+00, 3.500000e+00, 4.500000e+00, 5.500000e+00, 6.500000e+00, 7.500000e+00, 8.500000e+00, 9.500000e+00, 1.050000e+01, 1.150000e+01, 1.250000e+01, 1.350000e+01, 1.450000e+01, 1.550000e+01, 1.650000e+01, 1.750000e+01]> : tensor<16xf32>
  // CHECK: %0 = "tfl.depthwise_conv_2d"(%arg0, %arg1, %cst)
  // CHECK-SAME: fused_activation_function = "RELU6"
}

// CHECK-LABEL: intermOpUsedTwice
func @intermOpUsedTwice(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<16x3x3x3xf32>) -> (tensor<256x30x30x16xf32>, tensor<256x30x30x16xf32>) {
  %cst = constant dense<1.5> : tensor<16xf32>
  %cst_0 = constant dense<[1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0]> : tensor<16xf32>
  %0 = "tfl.conv_2d"(%arg0, %arg1, %cst_0) {dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  %1 = "tfl.add"(%0, %cst) {fused_activation_function = "RELU6"} : (tensor<256x30x30x16xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
  return %0, %1 : tensor<256x30x30x16xf32>, tensor<256x30x30x16xf32>

  // CHECK:  %cst = constant dense<[1.000000e+00, 2.000000e+00, 3.000000e+00, 4.000000e+00,
  // CHECK:  %cst_0 = constant dense<[2.500000e+00, 3.500000e+00, 4.500000e+00, 5.500000e+00,
  // CHECK:  %0 = "tfl.conv_2d"(%arg0, %arg1, %cst) {dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32}
  // CHECK:  %1 = "tfl.conv_2d"(%arg0, %arg1, %cst_0) {dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "RELU6", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32}
  // CHECK:  return %0, %1

}

// CHECK-LABEL: @fuseMulIntoDepthwiseConv2d
func @fuseMulIntoDepthwiseConv2d(%arg0: tensor<1x112x112x2xf32>) -> tensor<1x112x112x2xf32> {
  %cst0 = constant dense<[[[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0]], [[7.0, 8.0], [9.0, 10.0], [11.0, 12.0]], [[13.0, 14.0], [15.0, 16.0], [17.0, 18.0]]]]> : tensor<1x3x3x2xf32>
  %cst1 = constant dense<2.0> : tensor<2xf32>
  %cst2 = constant dense<[1.0, 2.0]> : tensor<2xf32>

  %0 = "tfl.depthwise_conv_2d"(%arg0, %cst0, %cst1) {depth_multiplier = 1 : i32, dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 1 : i32, stride_w = 1 : i32} : (tensor<1x112x112x2xf32>, tensor<1x3x3x2xf32>, tensor<2xf32>) -> tensor<1x112x112x2xf32>
  %1 = "tfl.mul"(%0, %cst2) {fused_activation_function = "RELU6"} : (tensor<1x112x112x2xf32>, tensor<2xf32>) -> tensor<1x112x112x2xf32>

  return %1 : tensor<1x112x112x2xf32>

// CHECK:  %cst = constant dense<{{\[\[\[\[}}1.000000e+00, 4.000000e+00], [3.000000e+00, 8.000000e+00], [5.000000e+00, 1.200000e+01]], {{\[\[}}7.000000e+00, 1.600000e+01], [9.000000e+00, 2.000000e+01], [1.100000e+01, 2.400000e+01]], {{\[\[}}1.300000e+01, 2.800000e+01], [1.500000e+01, 3.200000e+01], [1.700000e+01, 3.600000e+01]]]]> : tensor<1x3x3x2xf32>
// CHECK:  %cst_0 = constant dense<[2.000000e+00, 4.000000e+00]> : tensor<2xf32>
// CHECK:  %0 = "tfl.depthwise_conv_2d"(%arg0, %cst, %cst_0) {depth_multiplier = 1 : i32, dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "RELU6", padding = "SAME", stride_h = 1 : i32, stride_w = 1 : i32} : (tensor<1x112x112x2xf32>, tensor<1x3x3x2xf32>, tensor<2xf32>) -> tensor<1x112x112x2xf32>
// CHECK:  return %0
}

// CHECK-LABEL: @notFuseMulIntoDepthwiseConv2d
func @notFuseMulIntoDepthwiseConv2d(%arg0: tensor<1x112x112x2xf32>) -> tensor<1x112x112x2xf32> {
  %cst0 = constant dense<[[[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0]], [[7.0, 8.0], [9.0, 10.0], [11.0, 12.0]], [[13.0, 14.0], [15.0, 16.0], [17.0, 18.0]]]]> : tensor<1x3x3x2xf32>
  %cst1 = constant dense<2.0> : tensor<2xf32>
  %cst2 = constant dense<3.0> : tensor<112x2xf32>

  %0 = "tfl.depthwise_conv_2d"(%arg0, %cst0, %cst1) {depth_multiplier = 1 : i32, dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 1 : i32, stride_w = 1 : i32} : (tensor<1x112x112x2xf32>, tensor<1x3x3x2xf32>, tensor<2xf32>) -> tensor<1x112x112x2xf32>
  // We cannot fuse this tfl.mul into the preceding conv op becuase %cst2 is not broadcast-compatible to %cst0.
  %1 = "tfl.mul"(%0, %cst2) {fused_activation_function = "RELU6"} : (tensor<1x112x112x2xf32>, tensor<112x2xf32>) -> tensor<1x112x112x2xf32>

  return %1 : tensor<1x112x112x2xf32>

// CHECK:  %0 = "tfl.depthwise_conv_2d"(%arg0, %cst, %cst_0)
// CHECK:  %1 = "tfl.mul"(%0, %cst_1)
// CHECK:  return %1
}
