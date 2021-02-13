// RUN: tf-opt %s -split-input-file -verify-diagnostics | FileCheck %s

// Tests for TensorFlow ops with custom verifiers.

// TODO(hinsu): Remove tests for ops without custom verifiers. These tests were
// added along with manual op definition and are obsolete now that the op
// definitions are auto-generated.
// TODO(hinsu): Move attribute and type tests to types.mlir file.

//===--------------------------------------------------------------------===//
//  Test TF opaque attributes
//===--------------------------------------------------------------------===//

// CHECK-LABEL: func @opaquetensorattr
func @opaquetensorattr() -> () {
^bb0:
// CHECK: "tf.opaqueIntTensor"() {bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4xi32>} : () -> ()
  "tf.opaqueIntTensor"(){bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4xi32>} : () -> ()
// CHECK: "tf.opaqueFloatTensor"() {bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4xf32>} : () -> ()
  "tf.opaqueFloatTensor"(){bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4xf32>} : () -> ()
// CHECK: "tf.opaqueStringTensor"() {bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4x!tf.string>} : () -> ()
  "tf.opaqueStringTensor"(){bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4x!tf.string>} : () -> ()
// CHECK: "tf.opaqueResourceTensor"() {bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4x!tf.resource>} : () -> ()
  "tf.opaqueResourceTensor"(){bar = opaque<"tf", "0x68656C6C6F"> : tensor<2x1x4x!tf.resource>} : () -> ()
  return
}

//===--------------------------------------------------------------------===//
//  Test TF operations (tf.*)
//===--------------------------------------------------------------------===//

// -----

// CHECK-LABEL: func @testIdentity
func @testIdentity(%arg0: tensor<4x2x!tf.stringref>) -> tensor<4x2x!tf.string> {
  %0 = "tf.Identity"(%arg0) : (tensor<4x2x!tf.stringref>) -> tensor<4x2x!tf.string>
  return %0 : tensor<4x2x!tf.string>
}

// -----

// CHECK-LABEL: func @testBitcast
func @testBitcast(%arg0: tensor<3x4xui16>) -> tensor<3x4x!tf.quint16> {
  %0 = "tf.Bitcast"(%arg0) : (tensor<3x4xui16>) -> tensor<3x4x!tf.quint16>
  return %0 : tensor<3x4x!tf.quint16>
}

// -----

// CHECK-LABEL: func @testReverseV2
func @testReverseV2(%arg0: tensor<2x4x3xui8>, %arg1: tensor<1xi32>) -> tensor<2x4x3xui8> {
  %0 = "tf.ReverseV2"(%arg0, %arg1) : (tensor<2x4x3xui8>, tensor<1xi32>) -> tensor<2x4x3xui8>
  return %0 :  tensor<2x4x3xui8>
}

// -----

func @testIdentityWrongType(%arg0: tensor<4x2x!tf.string>) -> tensor<4x2x!tf.stringref> {
  // expected-error @+1 {{all operands and results to have compatible element}}
  %0 = "tf.Identity"(%arg0) : (tensor<4x2x!tf.string>) -> tensor<4x2x!tf.stringref>
  return %0 : tensor<4x2x!tf.stringref>
}

// -----

// TODO(hinsu): Move this to MLIR core once the test dialect have a custom type.

// Check that broadcastable trait accepts TF specific element type
// CHECK-LABEL: func @testAdd
func @testAdd(%arg0: tensor<4x2x!tf.string>, %arg1: tensor<2x!tf.string>) -> tensor<4x2x!tf.string> {
  %0 = "tf.Add"(%arg0, %arg1) : (tensor<4x2x!tf.string>, tensor<2x!tf.string>) -> tensor<4x2x!tf.string>
  return %0 : tensor<4x2x!tf.string>
}

// -----

// Valid BiasAdd operation.
func @testBiasAdd(%arg0: tensor<2x3x5x7xf32>, %arg1: tensor<7xf32>) -> tensor<2x3x5x7xf32> {
  %0 = "tf.BiasAdd"(%arg0, %arg1) {data_format = "NHWC"} : (tensor<2x3x5x7xf32>, tensor<7xf32>) -> tensor<2x3x5x7xf32>
  return %0 : tensor<2x3x5x7xf32>
}

// -----

func @testBiasAddNoDataFormatOk(tensor<1x32x32x16xf32>, tensor<16xf32>) -> tensor<1x32x32x16xf32> {
^bb0(%arg0: tensor<1x32x32x16xf32>, %arg1: tensor<16xf32>):
  %0 = "tf.BiasAdd"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT"}: (tensor<1x32x32x16xf32>, tensor<16xf32>) -> tensor<1x32x32x16xf32>
  return %0 : tensor<1x32x32x16xf32>
}

// -----

func @testBiasAddWrongDataFormat(tensor<1x32x32x16xf32>, tensor<16xf32>) -> tensor<1x32x32x16xf32> {
^bb0(%arg0: tensor<1x32x32x16xf32>, %arg1: tensor<16xf32>):
  // expected-error @+1 {{attribute 'data_format' failed to satisfy constraint: 'NHWC' or 'NCHW' convnet data format}}
  %0 = "tf.BiasAdd"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "HWCN"} : (tensor<1x32x32x16xf32>, tensor<16xf32>) -> tensor<1x32x32x16xf32>
  return %0 : tensor<1x32x32x16xf32>
}

// -----

func @testBiasAdd(%arg0: tensor<3xf32>, %arg1: tensor<3xf32>) -> tensor<3xf32> {
  // expected-error @+1 {{requires value operand to have rank at least two with `NHWC` data format}}
  %0 = "tf.BiasAdd"(%arg0, %arg1) {data_format = "NHWC"} : (tensor<3xf32>, tensor<3xf32>) -> tensor<3xf32>
  return %0 : tensor<3xf32>
}

// -----

func @testBiasAdd(%arg0: tensor<2x3xf32>, %arg1: tensor<3xf32>) -> tensor<2x3xf32> {
  // expected-error @+1 {{requires value operand to have rank at least three with `NCHW` data format}}
  %0 = "tf.BiasAdd"(%arg0, %arg1) {data_format = "NCHW"} : (tensor<2x3xf32>, tensor<3xf32>) -> tensor<2x3xf32>
  return %0 : tensor<2x3xf32>
}

// -----

func @testBiasAdd(%arg0: tensor<2x3x5x7xf32>, %arg1: tensor<5x7xf32>) -> tensor<2x3x5x7xf32> {
  // expected-error @+1 {{requires bias operand to have rank exactly one}}
  %0 = "tf.BiasAdd"(%arg0, %arg1) {data_format = "NHWC"} : (tensor<2x3x5x7xf32>, tensor<5x7xf32>) -> tensor<2x3x5x7xf32>
  return %0 : tensor<2x3x5x7xf32>
}

// -----

func @testBiasAdd(%arg0: tensor<2x3x5x7xf32>, %arg1: tensor<5xf32>) -> tensor<2x3x5x7xf32> {
  // expected-error @+1 {{requires channel dimension and feature dimension to match; found 7 and 5, respectively}}
  %0 = "tf.BiasAdd"(%arg0, %arg1) {data_format = "NHWC"} : (tensor<2x3x5x7xf32>, tensor<5xf32>) -> tensor<2x3x5x7xf32>
  return %0 : tensor<2x3x5x7xf32>
}

// -----

// Valid BiasAddGrad operation.
func @testBiasAddGrad(%arg0: tensor<2x3x5x7xf32>) -> tensor<7xf32> {
  %0 = "tf.BiasAddGrad"(%arg0) {data_format = "NHWC"} : (tensor<2x3x5x7xf32>) -> (tensor<7xf32>)
  return %0 : tensor<7xf32>
}

// -----

func @testBiasAddGrad(%arg0: tensor<3xf32>) -> tensor<3xf32> {
  // expected-error @+1 {{requires out_backprop operand to have rank at least two with `NHWC` data format}}
  %0 = "tf.BiasAddGrad"(%arg0) {data_format = "NHWC"} : (tensor<3xf32>) -> tensor<3xf32>
  return %0 : tensor<3xf32>
}

// -----

func @testBiasAddGrad(%arg0: tensor<2x3xf32>) -> tensor<3xf32> {
  // expected-error @+1 {{requires out_backprop operand to have rank at least three with `NCHW` data format}}
  %0 = "tf.BiasAddGrad"(%arg0) {data_format = "NCHW"} : (tensor<2x3xf32>) -> tensor<3xf32>
  return %0 : tensor<3xf32>
}

// -----

// Test valid tf.BroadcastGradientArgs
// CHECK-LABEL: func @testBroadcastGradientArgs
func @testBroadcastGradientArgs(%s0: tensor<4xi32>, %s1: tensor<4xi32>) -> (tensor<1xi32>, tensor<0xi32>) {
  %r0, %r1 = "tf.BroadcastGradientArgs"(%s0, %s1) : (tensor<4xi32>, tensor<4xi32>) -> (tensor<1xi32>, tensor<0xi32>)
  return %r0, %r1 : tensor<1xi32>, tensor<0xi32>
}

// -----

func @testBroadcastGradientArgsIncompatibleInputType(%s0: tensor<4xi32>, %s1: tensor<4xi64>) -> (tensor<1xi32>, tensor<0xi32>) {
  // expected-error @+1 {{requires the same element type for all operands and results}}
  %r0, %r1 = "tf.BroadcastGradientArgs"(%s0, %s1) : (tensor<4xi32>, tensor<4xi64>) -> (tensor<1xi32>, tensor<0xi32>)
  return %r0, %r1 : tensor<1xi32>, tensor<0xi32>
}

// -----

func @testBroadcastGradientArgsIncompatibleBroadcastShape() -> (tensor<1xi32>, tensor<0xi32>) {
  %s0 = "tf.Const"() {value = dense<[4, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
  %s1 = "tf.Const"() {value = dense<[2, 4]> : tensor<2xi32>} : () -> tensor<2xi32>
  // expected-error @+1 {{requires broadcast compatible shape tensors for 's0' and 's1', but got dense<[4, 1]> : tensor<2xi32> and dense<[2, 4]> : tensor<2xi32>}}
  %r0, %r1 = "tf.BroadcastGradientArgs"(%s0, %s1) : (tensor<2xi32>, tensor<2xi32>) -> (tensor<1xi32>, tensor<0xi32>)
  return %r0, %r1 : tensor<1xi32>, tensor<0xi32>
}

// -----

func @testBroadcastGradientArgsInvalidS0Rank() -> (tensor<2x2xi32>, tensor<0xi32>) {
  %s0 = "tf.Const"() {value = dense<[[4, 1], [2, 3]]> : tensor<2x2xi32>} : () -> tensor<2x2xi32>
  %s1 = "tf.Const"() {value = dense<[2, 4]> : tensor<2xi32>} : () -> tensor<2xi32>
  // expected-error @+1 {{failed to verify that operand 0 is 1-D}}
  %r0, %r1 = "tf.BroadcastGradientArgs"(%s0, %s1) : (tensor<2x2xi32>, tensor<2xi32>) -> (tensor<1xi32>, tensor<0xi32>)
  return %r0, %r1 : tensor<1xi32>, tensor<0xi32>
}

// -----

func @testBroadcastGradientArgsInvalidS1Rank() -> (tensor<2xi32>, tensor<i32>) {
  %s0 = "tf.Const"() {value = dense<[4, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
  %s1 = "tf.Const"() {value = dense<1> : tensor<i32>} : () -> tensor<i32>
  // expected-error @+1 {{failed to verify that operand 1 is 1-D}}
  %r0, %r1 = "tf.BroadcastGradientArgs"(%s0, %s1) : (tensor<2xi32>, tensor<i32>) -> (tensor<1xi32>, tensor<0xi32>)
  return %r0, %r1 : tensor<1xi32>, tensor<0xi32>
}

// -----

func @testBroadcastGradientArgsInvalidR0Rank() -> (tensor<2x2xi32>, tensor<0xi32>) {
  %s0 = "tf.Const"() {value = dense<[4, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
  %s1 = "tf.Const"() {value = dense<[4, 4]> : tensor<2xi32>} : () -> tensor<2xi32>
  // expected-error @+1 {{failed to verify that result 0 is 1-D}}
  %r0, %r1 = "tf.BroadcastGradientArgs"(%s0, %s1) : (tensor<2xi32>, tensor<2xi32>) -> (tensor<2x2xi32>, tensor<0xi32>)
  return %r0, %r1 : tensor<2x2xi32>, tensor<0xi32>
}

// -----

func @testBroadcastGradientArgsInvalidR1Rank(%s0: tensor<4xi32>, %s1: tensor<4xi32>) -> (tensor<1xi32>, tensor<i32>) {
  // expected-error @+1 {{failed to verify that result 1 is 1-D}}
  %r0, %r1 = "tf.BroadcastGradientArgs"(%s0, %s1) : (tensor<4xi32>, tensor<4xi32>) -> (tensor<1xi32>, tensor<i32>)
  return %r0, %r1 : tensor<1xi32>, tensor<i32>
}

// -----

func @testBroadcastGradientArgsInvalidR0Size() -> (tensor<0xi32>, tensor<0xi32>) {
  %s0 = "tf.Const"() {value = dense<[4, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
  %s1 = "tf.Const"() {value = dense<[4, 4]> : tensor<2xi32>} : () -> tensor<2xi32>
  // expected-error @+1 {{requires dimension 0 size of 'r0' to be 1 but got 0}}
  %r0, %r1 = "tf.BroadcastGradientArgs"(%s0, %s1) : (tensor<2xi32>, tensor<2xi32>) -> (tensor<0xi32>, tensor<0xi32>)
  return %r0, %r1 : tensor<0xi32>, tensor<0xi32>
}

// -----

func @testBroadcastGradientArgsInvalidR1Size() -> (tensor<0xi32>, tensor<3xi32>) {
  %s0 = "tf.Const"() {value = dense<[4, 4]> : tensor<2xi32>} : () -> tensor<2xi32>
  %s1 = "tf.Const"() {value = dense<[1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
  // expected-error @+1 {{requires dimension 0 size of 'r1' to be 2 but got 3}}
  %r0, %r1 = "tf.BroadcastGradientArgs"(%s0, %s1) : (tensor<2xi32>, tensor<2xi32>) -> (tensor<0xi32>, tensor<3xi32>)
  return %r0, %r1 : tensor<0xi32>, tensor<3xi32>
}

// -----

// Test valid tf.BroadcastTo
// CHECK-LABEL: func @testBroadcastTo(%arg0: tensor<16xf32>)
func @testBroadcastTo(%arg0: tensor<16xf32>) -> tensor<16x16x16x16xf32> {
  %cst = constant dense<16> : tensor<4xi32>
  %0 = "tf.BroadcastTo"(%arg0, %cst) : (tensor<16xf32>, tensor<4xi32>) -> tensor<16x16x16x16xf32>
  return %0 : tensor<16x16x16x16xf32>
}

// -----

// Test valid tf.LeakyRelu
// CHECK-LABEL: func @testLeakyRelu(%arg0: tensor<16xf32>)
func @testLeakyRelu(tensor<16xf32>) -> tensor<16xf32> {
^bb0(%arg0: tensor<16xf32>):
  %0 = "tf.LeakyRelu"(%arg0) {alpha = 0.2 : f32} : (tensor<16xf32>) -> tensor<16xf32>
  return %0 : tensor<16xf32>
}

// -----
func @testLeakyWrongAlphaType(tensor<16xf32>) -> tensor<16xf32> {
^bb0(%arg0: tensor<16xf32>):
  // expected-error @+1 {{attribute 'alpha' failed to satisfy constraint: 32-bit float}}
  %0 = "tf.LeakyRelu"(%arg0) {alpha = 1: i32}: (tensor<16xf32>) -> tensor<16xf32>
  return %0 : tensor<16xf32>
}

// -----

// Test tf.Min with complex numbers.
// Previous versions of tensorflow said complex numbers were allowed with
// tf.Min even though it doesn't make sense. The legalization of tf to xla
// requires that complex types are not allowed in tf.Min, so we have an
// explicit unit here to make sure that invariant is enforced.
func @testMinComplex(%arg0: tensor<4x8xcomplex<f32>>) -> tensor<4x1xcomplex<f32>> {
  %dimension = "tf.Const"() { value = dense<1> : tensor<1xi64> } : () -> tensor<1xi64>
  // expected-error@below {{'tf.Min' op operand #0 must be tensor of}}
  %0 = "tf.Min"(%arg0, %dimension) { keep_dims = true }: (tensor<4x8xcomplex<f32>>, tensor<1xi64>) -> tensor<4x1xcomplex<f32>>
  return %0 : tensor<4x1xcomplex<f32>>
}

// -----

// CHECK-LABEL: func @testMul
func @testMul(%arg0: tensor<2xui16>) -> (tensor<2xui16>) {
  %0 = "tf.Mul"(%arg0, %arg0) {T = "tfdtype$DT_UINT16", device = "/device:CPU:0", name = "Mul"} : (tensor<2xui16>, tensor<2xui16>) -> tensor<2xui16>
  return %0 : tensor<2xui16>
}

// -----

// Test error message for incompatible element types.
func @testIncompatibleElementTypes(%arg0: tensor<3x2xf32>, %arg1: tensor<3x2xf64>) -> (tensor<3x2xf32>) {
    // expected-error @+1 {{'tf.Mul' op requires compatible element types for all operands and results}}
  %0 = "tf.Mul"(%arg0, %arg1) : (tensor<3x2xf32>, tensor<3x2xf64>) -> tensor<3x2xf32>
  return %0 : tensor<3x2xf32>
}

// -----

// Test error message for incompatible element types.
func @testIncompatibleElementTypes(%arg0: tensor<3x2xf32>, %arg1: tensor<3x2xf32>) -> (tensor<3x2xf64>) {
    // expected-error @+1 {{'tf.Mul' op requires compatible element types for all operands and results}}
  %0 = "tf.Mul"(%arg0, %arg1) : (tensor<3x2xf32>, tensor<3x2xf32>) -> tensor<3x2xf64>
  return %0 : tensor<3x2xf64>
}

// -----

func @testPadRank1Paddings(%input: tensor<2xi64>) -> tensor<3xi64> {
  %paddings = "tf.Const"() {value = dense<[0, 1]> : tensor<2xi64>} : () -> tensor<2xi64>
  // expected-error @+1 {{failed to verify that operand 1 is 2-D}}
  %0 = "tf.Pad"(%input, %paddings) : (tensor<2xi64>, tensor<2xi64>) -> tensor<3xi64>
  return %0 : tensor<3xi64>
}

// -----

func @testPadV2Rank1Paddings(%input: tensor<2xi64>) -> tensor<3xi64> {
  %constant = "tf.Const"() {value = dense<1> : tensor<i64>} : () -> tensor<i64>
  %paddings = "tf.Const"() {value = dense<[0, 1]> : tensor<2xi64>} : () -> tensor<2xi64>
  // expected-error @+1 {{failed to verify that operand 1 is 2-D}}
  %0 = "tf.PadV2"(%input, %paddings, %constant) : (tensor<2xi64>, tensor<2xi64>, tensor<i64>) -> tensor<3xi64>
  return %0 : tensor<3xi64>
}

// -----

func @testMirrorPadRank1Paddings(%input: tensor<2xi64>) -> tensor<3xi64> {
  %paddings = "tf.Const"() {value = dense<[0, 1]> : tensor<2xi64>} : () -> tensor<2xi64>
  // expected-error @+1 {{failed to verify that operand 1 is 2-D}}
  %0 = "tf.MirrorPad"(%input, %paddings) { mode = "SYMMETRIC" }: (tensor<2xi64>, tensor<2xi64>) -> tensor<3xi64>
  return %0 : tensor<3xi64>
}

// -----

// CHECK-LABEL: func @testReshape(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>, %arg2: tensor<10000xf32>, %arg3: tensor<*xi32>)
func @testReshape(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>, %arg2: tensor<10000xf32>, %arg3: tensor<*xi32>) -> (tensor<100x100xf32>, tensor<*xf32>, tensor<100x100xf32>, tensor<100x100xf32>, tensor<*xf32>, tensor<*xf32>) {
  %shape1 = constant dense<100> : tensor<2xi32>
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<*xf32>, tensor<2xi32>) -> tensor<100x100xf32>
  %shape2 = "tf.Shape"(%arg0) : (tensor<*xf32>) -> tensor<?xi32>
  %r2 = "tf.Reshape"(%arg1, %shape2) : (tensor<*xf32>, tensor<?xi32>) -> tensor<*xf32>
  %r3 = "tf.Reshape"(%arg2, %shape1) : (tensor<10000xf32>, tensor<2xi32>) -> tensor<100x100xf32>
  %shape3 = constant dense<[-1, 100]> : tensor<2xi32>
  %r4 = "tf.Reshape"(%arg2, %shape3) : (tensor<10000xf32>, tensor<2xi32>) -> tensor<100x100xf32>
  %r5 = "tf.Reshape"(%arg0, %arg3) : (tensor<*xf32>, tensor<*xi32>) -> tensor<*xf32>
  %r6 = "tf.Reshape"(%arg2, %arg3) : (tensor<10000xf32>, tensor<*xi32>) -> tensor<*xf32>
  return %r1, %r2, %r3, %r4, %r5, %r6: tensor<100x100xf32>, tensor<*xf32>, tensor<100x100xf32>, tensor<100x100xf32>, tensor<*xf32>, tensor<*xf32>
}

// -----
// tf.Reshape with incorrect type.
func @testReshape(tensor<*xf32>, tensor<*xf32>) -> (tensor<100x100xf32>) {
^bb0(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>):
  %shape1 = constant dense<100.> : tensor<2xf32>
  // expected-error @+1 {{must be tensor of 32/64-bit signed integer values}}
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<*xf32>, tensor<2xf32>) -> tensor<100x100xf32>
  return %r1 : tensor<100x100xf32>
}

// -----
// tf.Reshape with incorrect element number.
func @testReshape(%arg0: tensor<10x10x10xf32>, %shape1: tensor<2xi32>) -> tensor<100x100xf32> {
  // expected-error @+1 {{requires 'output' number of elements to match 'tensor' number of elements, but got 10000 and 1000}}
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x10xf32>, tensor<2xi32>) -> tensor<100x100xf32>
  return %r1 : tensor<100x100xf32>
}

// -----
// tf.Reshape with incorrect shape operand rank.
func @testReshape(%arg0: tensor<10x10x10xf32>, %shape1: tensor<2x2xi32>) -> tensor<*xf32> {
  // expected-error @+1 {{requires 'shape' to be rank 1, but got 2}}
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x10xf32>, tensor<2x2xi32>) -> tensor<*xf32>
  return %r1 : tensor<*xf32>
}

// -----
// tf.Reshape with more than one -1 in the shape.
func @testReshape(%arg0: tensor<10x10x10x10xf32>) -> tensor<100x100xf32> {
  %shape1 = constant dense<-1> : tensor<2xi32>
  // expected-error @+1 {{requires 'shape' to have at most one dynamic dimension, but got multiple dynamic dimensions at indices 0 and 1}}
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x10x10xf32>, tensor<2xi32>) -> tensor<100x100xf32>
  return %r1 : tensor<100x100xf32>
}

// -----
// tf.Reshape with shape operand element < -1.
func @testReshape(%arg0: tensor<10x10x10x10xf32>) -> tensor<100x100xf32> {
  %shape1 = constant dense<[100, -2]> : tensor<2xi32>
  // expected-error @+1 {{requires 'shape' to have dimensions greater than -1, but got -2 at index 1}}
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x10x10xf32>, tensor<2xi32>) -> tensor<100x100xf32>
  return %r1 : tensor<100x100xf32>
}

// -----
// tf.Reshape with -1 in the shape can't infer the dimension.
func @testReshape(%arg0: tensor<10x10x10x10xf32>) -> tensor<100x100xf32> {
  %shape1 = constant dense<[101, -1]> : tensor<2xi32>
  // expected-error @+1 {{requires 'tensor' number of elements be a multiple of 101, but got 10000}}
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x10x10xf32>, tensor<2xi32>) -> tensor<100x100xf32>
  return %r1 : tensor<100x100xf32>
}

// -----
// tf.Reshape with incorrect output rank.
func @testReshape(%arg0: tensor<10x10xf32>) -> tensor<?x?xf32> {
  %shape1 = constant dense<[100]> : tensor<1xi32>
  // expected-error @+1 {{requires 'output' type 'tensor<?x?xf32>' to be cast compatible with expected type 'tensor<100xf32>'}}
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10xf32>, tensor<1xi32>) -> tensor<?x?xf32>
  return %r1 : tensor<?x?xf32>
}

// -----
// tf.Reshape with incorrect output dimension.
func @testReshape(%arg0: tensor<1000xf32>) -> tensor<?x8x?xf32> {
  %shape1 = constant dense<[10, 10, 10]> : tensor<3xi32>
  // expected-error @+1 {{requires 'output' type 'tensor<?x8x?xf32>' to be cast compatible with expected type 'tensor<10x10x10xf32>'}}
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<1000xf32>, tensor<3xi32>) -> tensor<?x8x?xf32>
  return %r1 : tensor<?x8x?xf32>
}

// -----
// tf.Reshape with a shape operand that has 0 for one of its elements.
func @testReshape(%arg0: tensor<10x10x10xf32>) -> tensor<?x0xf32> {
  %shape1 = constant dense<[-1, 0]> : tensor<2xi32>
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x10xf32>, tensor<2xi32>) -> tensor<?x0xf32>
  return %r1 : tensor<?x0xf32>
}

// -----
// tf.Reshape with a tensor operand that has 0 for one of its elements.
func @testReshape(%arg0: tensor<10x10x0xf32>) -> tensor<?x0xf32> {
  %shape1 = constant dense<[-1, 0]> : tensor<2xi32>
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x0xf32>, tensor<2xi32>) -> tensor<?x0xf32>
  return %r1 : tensor<?x0xf32>
}

// -----
// tf.Reshape with a tensor operand that has non-static shape.
func @testReshape(%arg0: tensor<10x10x?xf32>) -> tensor<10x10xf32> {
  %shape1 = constant dense<[10, 10]> : tensor<2xi32>
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x?xf32>, tensor<2xi32>) -> tensor<10x10xf32>
  return %r1 : tensor<10x10xf32>
}

// -----
// tf.Reshape with tensor operand that has non-static shape and shape operand
// with static shape.
func @testReshape(%arg0: tensor<10x10x?xf32>, %shape1: tensor<2xi32>) -> tensor<100x100xf32> {
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x?xf32>, tensor<2xi32>) -> tensor<100x100xf32>
  return %r1 : tensor<100x100xf32>
}

// -----
// tf.Reshape with tensor and shape operands with static shape.
func @testReshape(%arg0: tensor<10x10x10x10xf32>, %shape1: tensor<2xi32>) -> tensor<100x100xf32> {
  %r1 = "tf.Reshape" (%arg0, %shape1) : (tensor<10x10x10x10xf32>, tensor<2xi32>) -> tensor<100x100xf32>
  return %r1 : tensor<100x100xf32>
}

// -----

// CHECK-LABEL: func @testValidAvgPool
func @testValidAvgPool(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", ksize = [1, 7, 7, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

// CHECK-LABEL: func @testAvgPoolMissingDataFormatOk
func @testAvgPoolMissingDataFormatOk(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolWrongDataType(tensor<1x7x7x16xi32>) -> tensor<1x1x1x16xi32> {
^bb0(%arg0: tensor<1x7x7x16xi32>):
  // expected-error @+1 {{must be tensor of floating-point values}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_INT", data_format = "NHWC", ksize = [1, 7, 7, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xi32>) -> tensor<1x1x1x16xi32>
  return %0 : tensor<1x1x1x16xi32>
}

// -----

func @testAvgPoolWrongDataFormat(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{attribute 'data_format' failed to satisfy constraint: 'NHWC' or 'NCHW' convnet data format}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", data_format = "HWCN", ksize = [1, 7, 7, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolNoKsize(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{requires attribute 'ksize'}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolWrongKsizeCount(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{attribute 'ksize' failed to satisfy constraint: 64-bit integer array attribute with at least 4 elements}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [7, 7, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolWrongKsizeType(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{'ksize' failed to satisfy constraint: 64-bit integer array attribute with at least 4 elements}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7.5, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----
func @testAvgPoolWrongKsizeIntType(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{'ksize' failed to satisfy constraint: 64-bit integer array attribute with at least 4 elements}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1 : i32, 7 : i32, 7 : i32, 1 : i32], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolNoPadding(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{requires attribute 'padding'}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7, 1], strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolWrongPadding(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{attribute 'padding' failed to satisfy constraint: string attribute whose value is SAME, or VALID}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7, 1], padding = "MAGIC", strides = [1, 1, 1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolNoStrides(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{requires attribute 'strides'}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7, 1], padding = "VALID"} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolWrongStridesCount(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{attribute 'strides' failed to satisfy constraint: 64-bit integer array attribute with at least 4 elements}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7, 1], padding = "VALID", strides = [1, 1]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

func @testAvgPoolWrongStridesType(tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32> {
^bb0(%arg0: tensor<1x7x7x16xf32>):
  // expected-error @+1 {{attribute 'strides' failed to satisfy constraint: 64-bit integer array attribute with at least 4 elements}}
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", ksize = [1, 7, 7, 1], padding = "VALID", strides = ["1", "1", "1", "1"]} : (tensor<1x7x7x16xf32>) -> tensor<1x1x1x16xf32>
  return %0 : tensor<1x1x1x16xf32>
}

// -----

// CHECK-LABEL: func @testValidConv2D
func @testValidConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x32x32x16xf32> {
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x32x32x16xf32>
  return %0 : tensor<256x32x32x16xf32>
}

// -----

// CHECK-LABEL: func @testValidDynamicConv2D
func @testValidDynamicConv2D(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>) -> tensor<?x?x?x?xf32> {
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<*xf32>, tensor<*xf32>) -> tensor<?x?x?x?xf32>
  return %0 : tensor<?x?x?x?xf32>
}

// -----

// CHECK-LABEL: func @testValidConv3D
func @testValidConv3D(%arg0: tensor<256x32x32x32x3xf32>, %arg1: tensor<3x3x3x3x16xf32>) -> tensor<256x32x32x32x16xf32> {
  %0 = "tf.Conv3D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1, 1]} : (tensor<256x32x32x32x3xf32>, tensor<3x3x3x3x16xf32>) -> tensor<256x32x32x32x16xf32>
  return %0 : tensor<256x32x32x32x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x32x32x16xf32> {
  // expected-error @+1 {{requires operands to be 4D tensor}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x32x32x16xf32>
  return %0 : tensor<256x32x32x16xf32>
}

// -----

func @testConv3D(%arg0: tensor<256x32x32x32x3xf32>, %arg1: tensor<3x3x3x3x16xf32>) -> tensor<256x32x32x16xf32> {
  // expected-error @+1 {{op inferred type(s) 'tensor<256x32x32x32x16xf32>' are incompatible with return type(s) of operation 'tensor<256x32x32x16xf32>'}}
  %0 = "tf.Conv3D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1, 1]} : (tensor<256x32x32x32x3xf32>, tensor<3x3x3x3x16xf32>) -> tensor<256x32x32x16xf32>
  return %0 : tensor<256x32x32x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x2x16xf32>) -> tensor<256x32x32x16xf32> {
  // expected-error @+1 {{requires the number of input channels to be divisible by the number of filter input channels; found 3 and 2, respectively}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x2x16xf32>) -> tensor<256x32x32x16xf32>
  return %0 : tensor<256x32x32x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires attribute 'explicit_paddings'}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "EXPLICIT", strides = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires explicit_paddings attribute length to be 8}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "EXPLICIT", strides = [1, 1, 1, 1], explicit_paddings = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires non negative explicit paddings}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "EXPLICIT", strides = [1, 1, 1, 1], explicit_paddings = [0, 0, 1, -1, 1, -1, 0, 0]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires strides attribute length to be 4}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires positive strides}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [0, 1, 1, 0]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{op inferred type(s) 'tensor<256x16x11x16xf32>' are incompatible with return type(s) of operation 'tensor<256x30x30x16xf32>'}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 2, 3, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x16x30x16xf32> {
  // expected-error @+1 {{op inferred type(s) 'tensor<256x16x11x16xf32>' are incompatible with return type(s) of operation 'tensor<256x16x30x16xf32>'}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 2, 3, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x16x30x16xf32>
  return %0 : tensor<256x16x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x32x32x16xf32> {
  // expected-error @+1 {{op inferred type(s) 'tensor<256x6x6x16xf32>' are incompatible with return type(s) of operation 'tensor<256x32x32x16xf32>'}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "EXPLICIT", dilations = [1, 2, 3, 4], explicit_paddings = [1, 2, 3, 4, 5, 6, 7, 8], strides = [5, 6, 7, 8]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x32x32x16xf32>
  return %0 : tensor<256x32x32x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x32x32x16xf32> {
  // expected-error @+1 {{op inferred type(s) 'tensor<256x30x30x16xf32>' are incompatible with return type(s) of operation 'tensor<256x32x32x16xf32>'}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x32x32x16xf32>
  return %0 : tensor<256x32x32x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires dilations attribute length to be 4}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1], dilations = [1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testConv2D(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32> {
  // expected-error @+1 {{requires positive dilations}}
  %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1], dilations = [1, 1, 0, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %0 : tensor<256x30x30x16xf32>
}

// -----

func @testMaxPoolGrad(%orig_input: tensor<f32>, %orig_output: tensor<10x12x12x64xf32>, %grad: tensor<10x12x12x64xf32>) -> tensor<10x24x24x64xf32> {
  // expected-error @+1 {{requires orig_input to be rank 4}}
  %result = "tf.MaxPoolGrad"(%orig_input, %orig_output, %grad) {
     data_format = "NHWC",
     ksize = [1, 2, 2, 1],
     padding = "VALID",
     strides = [1, 2, 2, 1]
  } : (tensor<f32>, tensor<10x12x12x64xf32>, tensor<10x12x12x64xf32>) -> tensor<10x24x24x64xf32>
  return %result : tensor<10x24x24x64xf32>
}

// -----

func @testMaxPoolGrad(%orig_input: tensor<10x24x24x64xf32>, %orig_output: tensor<12x12x64xf32>, %grad: tensor<10x12x12x64xf32>) -> tensor<10x24x24x64xf32> {
  // expected-error @+1 {{requires orig_output to be rank 4}}
  %result = "tf.MaxPoolGrad"(%orig_input, %orig_output, %grad) {
     data_format = "NHWC",
     ksize = [1, 2, 2, 1],
     padding = "VALID",
     strides = [1, 2, 2, 1]
  } : (tensor<10x24x24x64xf32>, tensor<12x12x64xf32>, tensor<10x12x12x64xf32>) -> tensor<10x24x24x64xf32>
  return %result : tensor<10x24x24x64xf32>
}

// -----

func @testMaxPoolGrad(%orig_input: tensor<10x24x24x64xf32>, %orig_output: tensor<10x12x12x64xf32>, %grad: tensor<12x12x64xf32>) -> tensor<10x24x24x64xf32> {
  // expected-error @+1 {{requires grad to be rank 4}}
  %result = "tf.MaxPoolGrad"(%orig_input, %orig_output, %grad) {
     data_format = "NHWC",
     ksize = [1, 2, 2, 1],
     padding = "VALID",
     strides = [1, 2, 2, 1]
  } : (tensor<10x24x24x64xf32>, tensor<10x12x12x64xf32>, tensor<12x12x64xf32>) -> tensor<10x24x24x64xf32>
  return %result : tensor<10x24x24x64xf32>
}

// -----

// CHECK-LABEL: func @testValidDepthwiseConv2dNative
func @testValidDepthwiseConv2dNative(tensor<256x32x32x3xf32>, tensor<3x3x3x4xf32>) -> tensor<256x30x30x12xf32> {
^bb0(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x4xf32>) :
  %0 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {device = "", name = "MobilenetV2/expanded_conv/depthwise/depthwise", T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 1, 1, 1], padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x4xf32>) -> tensor<256x30x30x12xf32>
  return %0 : tensor<256x30x30x12xf32>
}

// -----

// Test valid tf.FakeQuantWithMinMaxArgs
// CHECK-LABEL: func @testValidFakeQuantWithMinMaxArgs
func @testValidFakeQuantWithMinMaxArgs(tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>):
  %0 = "tf.FakeQuantWithMinMaxArgs"(%arg0) {min = -1.0 : f32, max = 1.0 : f32, num_bits = 3} : (tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxArgs
func @testInvalidFakeQuantWithMinMaxArgsWrongAttr(tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>):
  // expected-error @+1 {{requires num_bits to be between 2 and 16, inclusive}}
  %0 = "tf.FakeQuantWithMinMaxArgs"(%arg0) {min = -1.0 : f32, max = 1.0 : f32, num_bits = 0} : (tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test valid tf.FakeQuantWithMinMaxVars
// CHECK-LABEL: func @testValidFakeQuantWithMinMaxVars
func @testValidFakeQuantWithMinMaxVars(tensor<8x8x8x8xf32>, tensor<f32>, tensor<f32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<f32>, %arg2: tensor<f32>):
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) : (tensor<8x8x8x8xf32>, tensor<f32>, tensor<f32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVars
func @testInvalidFakeQuantWithMinMaxVarsWrongAttr(tensor<8x8x8x8xf32>, tensor<f32>, tensor<f32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<f32>, %arg2: tensor<f32>):
  // expected-error @+1 {{requires num_bits to be between 2 and 16, inclusive}}
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) {min = -1.0 : f32, max = 1.0 : f32, num_bits = 0} : (tensor<8x8x8x8xf32>, tensor<f32>, tensor<f32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVars
func @testInvalidFakeQuantWithMinMaxVarsWrongMinRank(tensor<8x8x8x8xf32>, tensor<1xf32>, tensor<2xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<1xf32>, %arg2: tensor<2xf32>):
  // expected-error @+1 {{requires min to be a 0d float tensor}}
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) : (tensor<8x8x8x8xf32>, tensor<1xf32>, tensor<2xf32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVars
func @testInvalidFakeQuantWithMinMaxVarsWrongMaxRank(tensor<8x8x8x8xf32>, tensor<f32>, tensor<2xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<f32>, %arg2: tensor<2xf32>):
  // expected-error @+1 {{requires max to be a 0d float tensor}}
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) : (tensor<8x8x8x8xf32>, tensor<f32>, tensor<2xf32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVars
func @testInvalidFakeQuantWithMinMaxVarsWrongMinType(tensor<8x8x8x8xf32>, tensor<i32>, tensor<i32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<i32>, %arg2: tensor<i32>):
  // expected-error @+1 {{op operand #1 must be tensor of 32-bit float values}}
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) : (tensor<8x8x8x8xf32>, tensor<i32>, tensor<i32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVars
func @testInvalidFakeQuantWithMinMaxVarsWrongMaxType(tensor<8x8x8x8xf32>, tensor<f32>, tensor<i32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<f32>, %arg2: tensor<i32>):
  // expected-error @+1 {{op operand #2 must be tensor of 32-bit float values}}
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) : (tensor<8x8x8x8xf32>, tensor<f32>, tensor<i32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>
}

// -----

// Test valid tf.FakeQuantWithMinMaxVarsPerChannel
// CHECK-LABEL: func @FakeQuantWithMinMaxVarsPerChannel
func @FakeQuantWithMinMaxVarsPerChannel(tensor<1x2x3x8xf32>, tensor<8xf32>, tensor<8xf32>) -> tensor<1x2x3x8xf32> {
^bb0(%arg0: tensor<1x2x3x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>):
  %0 = "tf.FakeQuantWithMinMaxVarsPerChannel"(%arg0, %arg1, %arg2) : (tensor<1x2x3x8xf32>, tensor<8xf32>, tensor<8xf32>) -> tensor<1x2x3x8xf32>
  return %0 : tensor<1x2x3x8xf32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVarsPerChannel
func @FakeQuantWithMinMaxVarsPerChannel_ranked_inputs(tensor<f32>, tensor<8xf32>, tensor<8xf32>) -> tensor<f32> {
^bb0(%arg0: tensor<f32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>):
  // expected-error @+1 {{requires inputs to be at least 1d float tensor}}
  %0 = "tf.FakeQuantWithMinMaxVarsPerChannel"(%arg0, %arg1, %arg2) : (tensor<f32>, tensor<8xf32>, tensor<8xf32>) -> tensor<f32>
  return %0 : tensor<f32>
}

// -----

// Test invalid tf.FakeQuantWithMinMaxVarsPerChannel
func @FakeQuantWithMinMaxVarsPerChannel_mismatch_min_max(tensor<1x2x3x8xf32>, tensor<1xf32>, tensor<8xf32>) -> tensor<1x2x3x8xf32> {
^bb0(%arg0: tensor<1x2x3x8xf32>, %arg1: tensor<1xf32>, %arg2: tensor<8xf32>):
  // expected-error @+1 {{requires min and max to have same size as last dimension of inputs}}
  %0 = "tf.FakeQuantWithMinMaxVarsPerChannel"(%arg0, %arg1, %arg2) : (tensor<1x2x3x8xf32>, tensor<1xf32>, tensor<8xf32>) -> tensor<1x2x3x8xf32>
  return %0 : tensor<1x2x3x8xf32>
}

// -----

// Test invalid tf.Fill
func @testFill(tensor<i32>, tensor<f32>) -> tensor<?x?xf32> {
^bb0(%arg0: tensor<i32>, %arg1: tensor<f32>):
  // expected-error @+1 {{requires dims to be a 1D tensor}}
  %0 = "tf.Fill"(%arg0, %arg1) : (tensor<i32>, tensor<f32>) -> (tensor<?x?xf32>)
  return %0 : tensor<?x?xf32>
}

// -----

// Test invalid tf.Fill
func @testFill(tensor<2xi32>, tensor<1xf32>) -> tensor<?x?xf32> {
^bb0(%arg0: tensor<2xi32>, %arg1: tensor<1xf32>):
  // expected-error @+1 {{requires value to be a scalar}}
  %0 = "tf.Fill"(%arg0, %arg1) : (tensor<2xi32>, tensor<1xf32>) -> (tensor<?x?xf32>)
  return %0 : tensor<?x?xf32>
}

// -----

// Test valid tf.FusedBatchNorm
// CHECK-LABEL: func @testFusedBatchNorm
func @testFusedBatchNorm(tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<8xf32>):
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  return %0#0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FusedBatchNorm
func @testFusedBatchNormWrongXType(tensor<8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> tensor<8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<8xf32>):
  // expected-error @+1 {{requires x to be a 4D float tensor}}
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  return %0#0 : tensor<8x8x8xf32>
}

// -----

// Test invalid tf.FusedBatchNorm
func @testFusedBatchNormWrongScaleType(tensor<8x8x8x8xf32>, tensor<8xi32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xi32>, %arg2: tensor<8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<8xf32>):
  // expected-error @+1 {{operand #1 must be tensor of 32-bit float values}}
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xi32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xi32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  return %0#0 : tensor<8x8x8x8xf32>
}

// -----

// Test invalid tf.FusedBatchNorm
func @testFusedBatchNormWrongOffsetType(tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<2x8xf32>, tensor<8xf32>, tensor<8xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<2x8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<8xf32>):
  // expected-error @+1 {{requires offset to be a 1D float tensor}}
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<2x8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<2x8xf32>, tensor<8xf32>, tensor<8xf32>)
  return %0#0 : tensor<8x8x8x8xf32>
}

// -----
// Test invalid tf.FusedBatchNorm
func @testFusedBatchNormWrongMeanType(tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<?x8xf32>, tensor<8xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>, %arg3: tensor<?x8xf32>, %arg4: tensor<8xf32>):
  // expected-error @+1 {{requires mean to be a 1D float tensor}}
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<?x8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<?x8xf32>, tensor<8xf32>)
  return %0#0 : tensor<8x8x8x8xf32>
}

// -----
// Test invalid tf.FusedBatchNorm
func @testFusedBatchNormWrongVarianceType(tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<10x2xf32>) -> tensor<8x8x8x8xf32> {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<10x2xf32>):
  // expected-error @+1 {{requires variance to be a 1D float tensor}}
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<10x2xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<10x2xf32>)
  return %0#0 : tensor<8x8x8x8xf32>
}

// -----
func private @testIfThen(tensor<*xf32>) -> tensor<*xf32>
func private @testIfElse(tensor<*xf32>) -> tensor<*xf32>

// Test valid tf.If operation
// CHECK-LABEL: func @testValidIfOp
func @testValidIfOp(tensor<i1>, tensor<2xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<2xf32>):
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen, else_branch = @testIfElse, is_stateless = false
  } : (tensor<i1>, tensor<2xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

func private @testIfThen(f32) -> f32
func private @testIfElse(f32) -> f32

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, f32) -> f32 {
^bb0(%arg0: tensor<i1>, %arg1: f32):
  // expected-error @+1 {{operand #1 must be tensor of tf.dtype values}}
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen,
    else_branch = @testIfElse,
    is_stateless = false
  } : (tensor<i1>, f32) -> f32

  return %1 : f32
}

// -----

func private @testIfElse(tensor<2xf32>) -> tensor<2xf32>

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, tensor<2xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<2xf32>):
  // expected-error @+1 {{requires attribute 'then_branch'}}
  %1 = "tf.If"(%arg0, %arg1) {
    else_branch = @testIfElse, is_stateless = false
  } : (tensor<i1>, tensor<2xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

func private @testIfThen(tensor<2xf32>, tensor<2xf32>) -> tensor<2xf32>
func private @testIfElse(tensor<2xf32>) -> tensor<2xf32>

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, tensor<2xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<2xf32>):
  // expected-error @+1 {{'tf.If' op 'then_branch' inputs (size = 2) should have the same number of values as inputs (size = 1)}}
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen,
    else_branch = @testIfElse,
    is_stateless = false
  } : (tensor<i1>, tensor<2xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

func private @testIfThen(tensor<2xf32>) -> (tensor<2xf32>, tensor<2xf32>)
func private @testIfElse(tensor<2xf32>) -> tensor<2xf32>

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, tensor<2xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<2xf32>):
  // expected-error @+1 {{'tf.If' op 'then_branch' results (size = 2) should have the same number of values as results (size = 1)}}
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen,
    else_branch = @testIfElse,
    is_stateless = false
  } : (tensor<i1>, tensor<2xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

func private @testIfThen(tensor<*xf16>) -> tensor<*xf32>
func private @testIfElse(tensor<*xf32>) -> tensor<*xf32>

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, tensor<2xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<2xf32>):
  // expected-error @+1 {{'tf.If' op 'then_branch' input type tensor<*xf16> is incompatible with input type tensor<2xf32> at index 0}}
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen,
    else_branch = @testIfElse,
    is_stateless = false
  } : (tensor<i1>, tensor<2xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

func private @testIfThen(tensor<2xf32>) -> tensor<*xf32>
func private @testIfElse(tensor<3xf32>) -> tensor<*xf32>

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, tensor<*xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<*xf32>):
  // expected-error @+1 {{expects all branch input type(s) (tensor<2xf32>, tensor<3xf32>) at index 0 to be cast compatible}}
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen,
    else_branch = @testIfElse,
    is_stateless = false
  } : (tensor<i1>, tensor<*xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

func private @testIfThen(tensor<*xf32>) -> tensor<*xf32>
func private @testIfElse(tensor<*xf32>) -> tensor<3xf32>

// Test invalid tf.If operation
func @testInvalidIfOp(tensor<i1>, tensor<*xf32>) -> tensor<2xf32> {
^bb0(%arg0: tensor<i1>, %arg1: tensor<*xf32>):
  // expected-error @+1 {{'tf.If' op 'else_branch' result type tensor<3xf32> is incompatible with result type tensor<2xf32> at index 0}}
  %1 = "tf.If"(%arg0, %arg1) {
    then_branch = @testIfThen,
    else_branch = @testIfElse,
    is_stateless = false
  } : (tensor<i1>, tensor<*xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// -----

// Test invalid tf.Yield operation (parent should be IfRegion)
func @testInvalidYieldOp(%arg0: f32) -> () {
  // expected-error @+1 {{'tf.Yield' op expects parent op to be one of 'tf.CaseRegion, tf.IfRegion, tf.WhileRegion'}}
  "tf.Yield"(%arg0) : (f32) -> ()
}

// -----

// Test valid tf.IfRegion operation
// CHECK-LABEL: func @testValidIfRegionOp
func @testValidIfRegionOp(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  %neg = "tf.Neg"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
  %0 = "tf.IfRegion"(%arg0) ({
     %t = "tf.Abs"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }, {
     %e = "tf.Acos"(%neg) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%e) : (tensor<2xf32>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

// Test valid tf.IfRegion operation with multiple results
// CHECK-LABEL: func @testValidIfRegionOpWithMultipleResults
func @testValidIfRegionOpWithMultipleResults(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  %0, %1, %2 = "tf.IfRegion"(%arg0) ({
     %t0 = "tf.Abs"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     %t1 = "tf.Acos"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     %t2 = "tf.Acosh"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
    "tf.Yield"(%t0, %t1, %t2) : (tensor<2xf32>, tensor<2xf32>, tensor<2xf32>) -> ()
    }, {
     %e0 = "tf.Neg"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     %e1 = "tf.Relu"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     %e2 = "tf.Sin"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%e0, %e1, %e2) : (tensor<2xf32>, tensor<2xf32>, tensor<2xf32>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> (tensor<2xf32>, tensor<2xf32>, tensor<2xf32>)

  %3 = "tf.Add"(%0, %1) : (tensor<2xf32>, tensor<2xf32>) -> tensor<2xf32>
  %4 = "tf.Add"(%2, %3) : (tensor<2xf32>, tensor<2xf32>) -> tensor<2xf32>
  return %4 : tensor<2xf32>
}

// -----

// Test invalid type for operand #0 for tf.IfRegion operation
func @testInvalidIfRegionOpType0(%arg0: f32, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{operand #0 must be 0D tensor of 1-bit signless integer values, but got 'f32'}}
  %0 = "tf.IfRegion"(%arg0) ({
     %t = "tf.Abs"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }, {
     %e = "tf.Acos"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%e) : (tensor<2xf32>) -> ()
    }) { is_stateless = false} : (f32) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

// tf.IfRegion operation should have 2 regions
func @testInvalidIfRegionOp1Region(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{op expected 2 regions}}
  %0 = "tf.IfRegion"(%arg0) ({
     %t = "tf.Abs"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

func @testInvalidIfRegionOpNoRegions(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{op expected 2 regions}}
  %0 = "tf.IfRegion"(%arg0) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

func @testInvalidIfRegionOp3Regions(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{op expected 2 regions}}
  %0 = "tf.IfRegion"(%arg0) ({
     %t = "tf.Abs"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }, {
     %te = "tf.Relu"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%te) : (tensor<2xf32>) -> ()
    }, {
     %e = "tf.Acos"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%e) : (tensor<2xf32>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

// tf.IfRegion regions should be terminated with a tf.Yield
func @testIfRegionThenTerminator(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+2 {{'tf.IfRegion' op expects regions to end with 'tf.Yield'}}
  // expected-note @+1 {{in custom textual format, the absence of terminator implies 'tf.Yield'}}
  %0 = "tf.IfRegion"(%arg0) ({
     %t = "tf.Abs"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
   }, {
     %e = "tf.Acos"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%e) : (tensor<2xf32>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

func @testIfRegionElseTerminator(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+2 {{'tf.IfRegion' op expects regions to end with 'tf.Yield'}}
  // expected-note @+1 {{in custom textual format, the absence of terminator implies 'tf.Yield'}}
  %0 = "tf.IfRegion"(%arg0) ({
     %t = "tf.Abs"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }, {
     %e = "tf.Acos"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

// tf.Region yield number of results should match op number of results
func @testIfRegionThenResultCount(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{'tf.IfRegion' op then results (size = 2) should have the same number of values as results (size = 1)}}
  %0 = "tf.IfRegion"(%arg0) ({
     %t = "tf.Abs"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%t, %t) : (tensor<2xf32>, tensor<2xf32>) -> ()
    }, {
     %e = "tf.Acos"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%e) : (tensor<2xf32>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

func @testIfRegionElseResultCount(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{'tf.IfRegion' op else results (size = 2) should have the same number of values as results (size = 1)}}
  %0 = "tf.IfRegion"(%arg0) ({
     %t = "tf.Abs"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }, {
     %e = "tf.Acos"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%e, %e) : (tensor<2xf32>, tensor<2xf32>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

// tf.IfRegion yield types should match op result types
func @testIfRegionOpYieldMismatchThen(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{'tf.IfRegion' op then result type tensor<i1> is incompatible with result type tensor<2xf32> at index 0}}
  %0 = "tf.IfRegion"(%arg0) ({
     "tf.Yield"(%arg0) : (tensor<i1>) -> ()
    }, {
     %e = "tf.Acos"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%e) : (tensor<2xf32>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

func @testIfRegionOpYieldMismatchElse(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{'tf.IfRegion' op else result type tensor<i1> is incompatible with result type tensor<2xf32> at index 0}}
  %0 = "tf.IfRegion"(%arg0) ({
     %t = "tf.Acos"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }, {
     "tf.Yield"(%arg0) : (tensor<i1>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

// value generated in one branch cannot be consumed in the other branch
func @testIfRegionElseConsumingThen(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  %0 = "tf.IfRegion"(%arg0) ({
     %t = "tf.Acos"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }, {
     // expected-error @+1 {{use of undeclared SSA value name}}
     "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

func @testIfRegionThenConsumingElse(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
   %0 = "tf.IfRegion"(%arg0) ({
     // expected-error @+1 {{does not dominate this use}}
     "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }, {
      // expected-note @+1 {{operand defined here}}
      %t = "tf.Acos"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
      "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

// The regions for IfRegion themselves cannot have any arguments
func @testInvalidIfRegionThenArg(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  %neg = "tf.Neg"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
  // expected-error @+1 {{'tf.IfRegion' op region #0 should have no arguments}}
  %0 = "tf.IfRegion"(%arg0) ({
     ^bb(%arg_bb: tensor<2xf32>):
     %t = "tf.Abs"(%arg_bb) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }, {
     %e = "tf.Acos"(%neg) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%e) : (tensor<2xf32>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

func @testInvalidIfRegionElseArg(%arg0: tensor<i1>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  %neg = "tf.Neg"(%arg1) : (tensor<2xf32>) -> tensor<2xf32>
  // expected-error @+1 {{'tf.IfRegion' op region #1 should have no arguments}}
  %0 = "tf.IfRegion"(%arg0) ({
     %t = "tf.Abs"(%neg) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%t) : (tensor<2xf32>) -> ()
    }, {
     ^bb(%arg_bb: tensor<2xf32>):
     %e = "tf.Acos"(%arg_bb) : (tensor<2xf32>) -> tensor<2xf32>
     "tf.Yield"(%e) : (tensor<2xf32>) -> ()
    }) { is_stateless = false} : (tensor<i1>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// -----

// Test valid tf.MatrixBandPart
// CHECK-LABEL: func @testValidMatrixBandPartOp
func @testValidMatrixBandPartOp(%arg0: tensor<64x64xbf16>, %arg1: tensor<i64>, %arg2: tensor<i64>) -> tensor<64x64xbf16> {
  %0 = "tf.MatrixBandPart"(%arg0, %arg1, %arg2) : (tensor<64x64xbf16>, tensor<i64>, tensor<i64>) -> tensor<64x64xbf16>
  return %0 : tensor<64x64xbf16>
}

// -----

// Test valid tf.MatrixBandPart
// CHECK-LABEL: func @testValidMatrixBandPartOp3D
func @testValidMatrixBandPartOp3D(%arg0: tensor<64x64x64xbf16>, %arg1: tensor<i64>, %arg2: tensor<i64>) -> tensor<64x64x64xbf16> {
  %0 = "tf.MatrixBandPart"(%arg0, %arg1, %arg2) : (tensor<64x64x64xbf16>, tensor<i64>, tensor<i64>) -> tensor<64x64x64xbf16>
  return %0 : tensor<64x64x64xbf16>
}

// -----

// Test valid tf.MatrixBandPart
// CHECK-LABEL: func @testValidMatrixBandPartOpUnranked
func @testValidMatrixBandPartOpUnranked(%arg0: tensor<*xbf16>, %arg1: tensor<i64>, %arg2: tensor<i64>) -> tensor<*xbf16> {
  %0 = "tf.MatrixBandPart"(%arg0, %arg1, %arg2) : (tensor<*xbf16>, tensor<i64>, tensor<i64>) -> tensor<*xbf16>
  return %0 : tensor<*xbf16>
}

// -----

// Test valid tf.MatrixBandPart
// CHECK-LABEL: func @testValidMatrixBandPartOpUnrankedBand
func @testValidMatrixBandPartOpUnrankedBand(%arg0: tensor<64x64x64xbf16>, %arg1: tensor<i64>, %arg2: tensor<i64>) -> tensor<*xbf16> {
  %0 = "tf.MatrixBandPart"(%arg0, %arg1, %arg2) : (tensor<64x64x64xbf16>, tensor<i64>, tensor<i64>) -> tensor<*xbf16>
  return %0 : tensor<*xbf16>
}

// -----

// Test valid tf.MatrixBandPart
// CHECK-LABEL: func @testValidMatrixBandPartOpCompatibleDynamicShapes
func @testValidMatrixBandPartOpCompatibleDynamicShapes(%arg0: tensor<?x10x?xbf16>, %arg1: tensor<i64>, %arg2: tensor<i64>) -> tensor<?x?x8xbf16> {
  %0 = "tf.MatrixBandPart"(%arg0, %arg1, %arg2) : (tensor<?x10x?xbf16>, tensor<i64>, tensor<i64>) -> tensor<?x?x8xbf16>
  return %0 : tensor<?x?x8xbf16>
}

// -----

// Test invalid tf.MatrixBandPart
func @testInvalidMatrixBandPartOp(%arg0: tensor<64x64x64xbf16>, %arg1: tensor<i64>, %arg2: tensor<i64>) -> tensor<64x64xbf16> {
  // expected-error @+1 {{op failed to verify that all of {input, band} have dynamically equal types}}
  %0 = "tf.MatrixBandPart"(%arg0, %arg1, %arg2) : (tensor<64x64x64xbf16>, tensor<i64>, tensor<i64>) -> tensor<64x64xbf16>
  return %0 : tensor<64x64xbf16>
}

// -----

// Test invalid tf.MatrixBandPart
func @testInvalidMatrixBandPartOp(%arg0: tensor<i64>, %arg1: tensor<64x64xi64>, %arg2: tensor<i64>) -> tensor<i64> {
  // expected-error @+1 {{op requires `input` to have rank of at least 2, but found 'tensor<i64>'}}
  %0 = "tf.MatrixBandPart"(%arg0, %arg1, %arg2) : (tensor<i64>, tensor<64x64xi64>, tensor<i64>) -> tensor<i64>
  return %0 : tensor<i64>
}

// -----

// Test invalid tf.MatrixBandPart
func @testInvalidMatrixBandPartOp(%arg0: tensor<64x64xi64>, %arg1: tensor<32xi64>, %arg2: tensor<i64>) -> tensor<64x64xi64> {
  // expected-error @+1 {{op requires `num_lower` to have 0 dimensions, but found 'tensor<32xi64>'}}
  %0 = "tf.MatrixBandPart"(%arg0, %arg1, %arg2) : (tensor<64x64xi64>, tensor<32xi64>, tensor<i64>) -> tensor<64x64xi64>
  return %0 : tensor<64x64xi64>
}

// -----

// Test invalid tf.MatrixBandPart
func @testInvalidMatrixBandPartOp(%arg0: tensor<64x64xi64>, %arg1: tensor<i64>, %arg2: tensor<32xi64>) -> tensor<64x64xi64> {
  // expected-error @+1 {{op requires `num_upper` to have 0 dimensions, but found 'tensor<32xi64>'}}
  %0 = "tf.MatrixBandPart"(%arg0, %arg1, %arg2) : (tensor<64x64xi64>, tensor<i64>, tensor<32xi64>) -> tensor<64x64xi64>
  return %0 : tensor<64x64xi64>
}

// -----

//===--------------------------------------------------------------------===//
//  tf.{|Stateful}PartitionedCall
//===--------------------------------------------------------------------===//

// Test valid tf.PartitionedCall
// CHECK-LABEL: func @testValidPartitionedCall
func @testValidPartitionedCall(%arg0: tensor<i32>) -> tensor<i32> {
  %0 = "tf.PartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @pcall_func} : (tensor<i32>) -> (tensor<i32>)
  return %0 : tensor<i32>
}

func @pcall_func(%arg0: tensor<i32>) -> tensor<i32> {
  return %arg0 : tensor<i32>
}

// -----

// Test invalid tf.PartitionedCall
func @testUndefinedPartitionedCall(%arg0: tensor<i32>) -> tensor<i32> {
  // expected-error @+1 {{'f' attribute refers to an undefined function: @nonexistant_pcall_func}}
  %0 = "tf.PartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @nonexistant_pcall_func} : (tensor<i32>) -> (tensor<i32>)
  return %0 : tensor<i32>
}

// -----

// Test invalid tf.PartitionedCall
func @testInvalidPartitionedCall(%arg0: tensor<i32>) -> tensor<i32> {
  // expected-error @+1 {{argument count mismatch: 'args' has 1 arguments, but '@pcall_func_2' expects 2}}
  %0 = "tf.PartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @pcall_func_2} : (tensor<i32>) -> (tensor<i32>)
  return %0 : tensor<i32>
}

func @pcall_func_2(%arg0: tensor<i32>, %arg1: tensor<i32>) -> tensor<i32> {
  return %arg0 : tensor<i32>
}

// -----

// Test valid tf.StatefulPartitionedCall
// CHECK-LABEL: func @testValidStatefulPartitionedCall
func @testValidStatefulPartitionedCall(%arg0: tensor<i32>) -> tensor<i32> {
  %0 = "tf.StatefulPartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @pcall_func} : (tensor<i32>) -> (tensor<i32>)
  return %0 : tensor<i32>
}

func @pcall_func(%arg0: tensor<i32>) -> tensor<i32> {
  return %arg0 : tensor<i32>
}

// -----

func @testUndefinedCallee(%arg0: tensor<i32>) -> tensor<i32> {
  // expected-error @+1 {{'f' attribute refers to an undefined function: @nonexistant_pcall_func}}
  %0 = "tf.StatefulPartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @nonexistant_pcall_func} : (tensor<i32>) -> (tensor<i32>)
  return %0 : tensor<i32>
}

// -----

func @testArgMismatch(%arg0: tensor<i32>) -> tensor<i32> {
  // expected-error @+1 {{argument count mismatch: 'args' has 1 arguments, but '@pcall_func_2' expects 2}}
  %0 = "tf.StatefulPartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @pcall_func_2} : (tensor<i32>) -> (tensor<i32>)
  return %0 : tensor<i32>
}

func @pcall_func_2(%arg0: tensor<i32>, %arg1: tensor<i32>) -> tensor<i32> {
  return %arg0 : tensor<i32>
}

// -----

//===--------------------------------------------------------------------===//
//  tf.Select
//===--------------------------------------------------------------------===//

// Test valid tf.Select
// CHECK-LABEL: func @testSelect
func @testSelect(%arg0: tensor<3xi1>, %arg1: tensor<3x2xf16>, %arg2: tensor<3x2xf16>) -> tensor<3x2xf16> {
  %0 = "tf.Select"(%arg0, %arg1, %arg2) : (tensor<3xi1>, tensor<3x2xf16>, tensor<3x2xf16>) -> tensor<3x2xf16>
  return %0: tensor<3x2xf16>
}

// -----

func @testInvalidSelect(%arg0: tensor<3xi1>, %arg1: tensor<2x3xf16>, %arg2: tensor<2x3xf16>) -> tensor<2x3xf16> {
  // expected-error @+1 {{requires that, when pred is a vector, the shape matches the first dimension of t and e}}
  %0 = "tf.Select"(%arg0, %arg1, %arg2) : (tensor<3xi1>, tensor<2x3xf16>, tensor<2x3xf16>) -> tensor<2x3xf16>
  return %0: tensor<2x3xf16>
}

// -----

// Test invalid tf.Select - broadcasting then/else parameters is not supported
func @selectBroadcastThen(%arg0: tensor<i1>, %arg1: tensor<8x1xi32>, %arg2: tensor<2x8x8xi32>) -> tensor<2x8x8xi32> {
  // expected-error @+1 {{requires t and e have compatible shapes}}
  %0 = "tf.Select"(%arg0, %arg1, %arg2) : (tensor<i1>, tensor<8x1xi32>, tensor<2x8x8xi32>) -> tensor<2x8x8xi32>
  return %0: tensor<2x8x8xi32>
}

// -----

func @invalidSelect(%arg0: tensor<2xi1>, %arg1: tensor<i32>, %arg2: tensor<i32>) -> tensor<2xi32> {
  // expected-error @+1 {{requires that t and e are nonscalar when pred is a vector}}
  %0 = "tf.Select"(%arg0, %arg1, %arg2) : (tensor<2xi1>, tensor<i32>, tensor<i32>) -> tensor<2xi32>
  return %0: tensor<2xi32>
}

// -----

func @invalidSelect(%arg0: tensor<1x8xi1>, %arg1: tensor<1x8x8xi32>, %arg2: tensor<1x8x8xi32>) -> tensor<1x8x8xi32> {
  // expected-error @+1 {{requires that pred is a scalar OR has the same rank as t and e OR is a vector}}
  %0 = "tf.Select"(%arg0, %arg1, %arg2) : (tensor<1x8xi1>, tensor<1x8x8xi32>, tensor<1x8x8xi32>) -> tensor<1x8x8xi32>
  return %0: tensor<1x8x8xi32>
}

// -----

//===--------------------------------------------------------------------===//
//  tf.SelectV2
//===--------------------------------------------------------------------===//

// Test valid tf.SelectV2
// CHfaECK-LABEL: func @selectV2BroadcastThen
func @selectV2BroadcastThen(%arg0: tensor<i1>, %arg1: tensor<8x1xi32>, %arg2: tensor<2x8x8xi32>) -> tensor<2x8x8xi32> {
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2) : (tensor<i1>, tensor<8x1xi32>, tensor<2x8x8xi32>) -> tensor<2x8x8xi32>
  return %0: tensor<2x8x8xi32>
}

// -----

// Test valid tf.SelectV2
// CHECK-LABEL: func @selectV2BroadcastElse
func @selectV2BroadcastElse(%arg0: tensor<i1>, %arg1: tensor<2x8x8xi32>, %arg2: tensor<8x1xi32>) -> tensor<2x8x8xi32> {
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2) : (tensor<i1>, tensor<2x8x8xi32>, tensor<8x1xi32>) -> tensor<2x8x8xi32>
  return %0: tensor<2x8x8xi32>
}

// -----

// Test valid tf.SelectV2
// CHECK-LABEL: func @selectV2BroadcastPred
func @selectV2BroadcastPred(%arg0: tensor<1xi1>, %arg1: tensor<2x8x8xi32>, %arg2: tensor<2x8x8xi32>) -> tensor<2x8x8xi32> {
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2) : (tensor<1xi1>, tensor<2x8x8xi32>, tensor<2x8x8xi32>) -> tensor<2x8x8xi32>
  return %0: tensor<2x8x8xi32>
}

// -----

// CHECK-LABEL: func @selectV2BroadcastAll
func @selectV2BroadcastAll(%arg0: tensor<8x1x1xi1>, %arg1: tensor<1x8x1xi32>, %arg2: tensor<1x1x8xi32>) -> tensor<8x8x8xi32> {
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2) : (tensor<8x1x1xi1>, tensor<1x8x1xi32>, tensor<1x1x8xi32>) -> tensor<8x8x8xi32>
  return %0: tensor<8x8x8xi32>
}

// -----

// CHECK-LABEL: func @selectV2DynamicRanked
func @selectV2DynamicRanked(%arg0: tensor<1xi1>, %arg1: tensor<2x?x8xi32>, %arg2: tensor<2x8x8xi32>) -> tensor<2x?x8xi32> {
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2) : (tensor<1xi1>, tensor<2x?x8xi32>, tensor<2x8x8xi32>) -> tensor<2x?x8xi32>
  return %0: tensor<2x?x8xi32>
}

// -----

// CHECK-LABEL: func @selectV2Unranked
func @selectV2Unranked(%arg0: tensor<1xi1>, %arg1: tensor<2x8x8xi32>, %arg2: tensor<*xi32>) -> tensor<*xi32> {
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2) : (tensor<1xi1>, tensor<2x8x8xi32>, tensor<*xi32>) -> tensor<*xi32>
  return %0: tensor<*xi32>
}

// -----

// Test invalid tf.SelectV2: this is an invalid broadcast for the predicate
func @testInvalidSelectV2(%arg0: tensor<3xi1>, %arg1: tensor<3x2xf16>, %arg2: tensor<3x2xf16>) -> tensor<3x2xf16> {
  // expected-error @+1 {{operands don't have broadcast-compatible shapes}}
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2) : (tensor<3xi1>, tensor<3x2xf16>, tensor<3x2xf16>) -> tensor<3x2xf16>
  return %0: tensor<3x2xf16>
}

// -----

//===--------------------------------------------------------------------===//
//  tf.Softmax
//===--------------------------------------------------------------------===//

// Test valid tf.Softmax
// CHECK-LABEL: func @testSoftmax
func @testSoftmax(tensor<8x16xf32>) -> tensor<8x16xf32> {
^bb0(%arg0: tensor<8x16xf32>):
  %0 = "tf.Softmax"(%arg0) {T = "tfdtype$DT_FLOAT"} : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>
}

// -----

// Test invalid tf.Softmax
func @testSoftmax(%arg0 : tensor<f32>) -> tensor<f32> {
  // expected-error @+1 {{requires operand to have rank at least 1}}
  %0 = "tf.Softmax"(%arg0) {T = "tfdtype$DT_FLOAT"} : (tensor<f32>) -> tensor<f32>
  return %0 : tensor<f32>
}

// -----

// Test valid tf.SoftmaxCrossEntropyWithLogits
// CHECK-LABEL: func @testSoftmaxCrossEntropyWithLogits
func @testSoftmaxCrossEntropyWithLogits(%arg0: tensor<2x3xf32>, %arg1: tensor<3xf32>) -> (tensor<3xf32>, tensor<2x3xf32>) {
  %0:2 = "tf.SoftmaxCrossEntropyWithLogits"(%arg0, %arg1) : (tensor<2x3xf32>, tensor<3xf32>) -> (tensor<3xf32>, tensor<2x3xf32>)
  return %0#0, %0#1 : tensor<3xf32>, tensor<2x3xf32>
}

// -----

func @testSoftmaxCrossEntropyWithLogits(%arg0: tensor<2xf32>, %arg1: tensor<3xf32>) -> (tensor<3xf32>, tensor<3xf32>) {
  // expected-error @+1 {{requires features and labels to be broadcast compatible to rank two}}
  %0:2 = "tf.SoftmaxCrossEntropyWithLogits"(%arg0, %arg1) : (tensor<2xf32>, tensor<3xf32>) -> (tensor<3xf32>, tensor<3xf32>)
  return %0#0, %0#1 : tensor<3xf32>, tensor<3xf32>
}

// -----

func @testSoftmaxCrossEntropyWithLogits(%arg0: tensor<3xf32>, %arg1: tensor<3xf32>) -> (tensor<3xf32>, tensor<3xf32>) {
  // expected-error @+1 {{requires features and labels to be broadcast compatible to rank two}}
  %0:2 = "tf.SoftmaxCrossEntropyWithLogits"(%arg0, %arg1) : (tensor<3xf32>, tensor<3xf32>) -> (tensor<3xf32>, tensor<3xf32>)
  return %0#0, %0#1 : tensor<3xf32>, tensor<3xf32>
}

// -----

//===--------------------------------------------------------------------===//
//  tf.SpaceToBatchND
//===--------------------------------------------------------------------===//

// Test valid tf.SpaceToBatchND
// CHECK-LABEL: func @testSpaceToBatchND
func @testSpaceToBatchND(%input: tensor<3x5x7x10xf32>, %block_shape: tensor<2xi64>, %paddings: tensor<2x2xi64>) -> tensor<?x?x?x10xf32> {
  %0 = "tf.SpaceToBatchND"(%input, %block_shape, %paddings) : (tensor<3x5x7x10xf32>, tensor<2xi64>, tensor<2x2xi64>) -> tensor<?x?x?x10xf32>
  return %0 : tensor<?x?x?x10xf32>
}

// -----

// Test valid tf.SpaceToBatchND
// CHECK-LABEL: func @testSpaceToBatchND
func @testSpaceToBatchND(%input: tensor<3x5x7x10xf32>) -> tensor<36x2x3x10xf32> {
  %block_shape = "tf.Const"() {value = dense<[4, 3]> : tensor<2xi64>} : () -> tensor<2xi64>
  %paddings = "tf.Const"() {value = dense<[[1, 2], [1, 1]]> : tensor<2x2xi64>} : () -> tensor<2x2xi64>
  %0 = "tf.SpaceToBatchND"(%input, %block_shape, %paddings) : (tensor<3x5x7x10xf32>, tensor<2xi64>, tensor<2x2xi64>) -> tensor<36x2x3x10xf32>
  return %0 : tensor<36x2x3x10xf32>
}

// -----

// Test invalid tf.SpaceToBatchND
func @testSpaceToBatchND(%input: tensor<3x5x7x10xf32>, %block_shape: tensor<2x2xi64>, %paddings: tensor<2x2xi64>) -> tensor<?x?x?x10xf32> {
  // expected-error @+1 {{requires rank of block_shape = 1; got 2}}
  %0 = "tf.SpaceToBatchND"(%input, %block_shape, %paddings) : (tensor<3x5x7x10xf32>, tensor<2x2xi64>, tensor<2x2xi64>) -> tensor<?x?x?x10xf32>
  return %0 : tensor<?x?x?x10xf32>
}

// -----

// Test invalid tf.SpaceToBatchND
func @testSpaceToBatchND(%input: tensor<3x5x7x10xf32>, %block_shape: tensor<2xi64>, %paddings: tensor<2xi64>) -> tensor<?x?x?x10xf32> {
  // expected-error @+1 {{requires rank of paddings = 2; got 1}}
  %0 = "tf.SpaceToBatchND"(%input, %block_shape, %paddings) : (tensor<3x5x7x10xf32>, tensor<2xi64>, tensor<2xi64>) -> tensor<?x?x?x10xf32>
  return %0 : tensor<?x?x?x10xf32>
}

// -----

// Test invalid tf.SpaceToBatchND
func @testSpaceToBatchND(%input: tensor<3x5x7x10xf32>, %block_shape: tensor<2xi64>, %paddings: tensor<2x10xi64>) -> tensor<?x?x?x10xf32> {
  // expected-error @+1 {{requires paddings.shape[1] to be 2; got 10}}
  %0 = "tf.SpaceToBatchND"(%input, %block_shape, %paddings) : (tensor<3x5x7x10xf32>, tensor<2xi64>, tensor<2x10xi64>) -> tensor<?x?x?x10xf32>
  return %0 : tensor<?x?x?x10xf32>
}

// -----

// Test invalid tf.SpaceToBatchND
func @testSpaceToBatchND(%input: tensor<3x5x7x10xf32>, %block_shape: tensor<4xi64>, %paddings: tensor<2x2xi64>) -> tensor<?x?x?x?xf32> {
  // expected-error @+1 {{requires block_shape.shape[0] must equal paddings.shape[0]}}
  %0 = "tf.SpaceToBatchND"(%input, %block_shape, %paddings) : (tensor<3x5x7x10xf32>, tensor<4xi64>, tensor<2x2xi64>) -> tensor<?x?x?x?xf32>
  return %0 : tensor<?x?x?x?xf32>
}

// -----

// Test invalid tf.SpaceToBatchND
func @testSpaceToBatchND(%input: tensor<3x5xf32>, %block_shape: tensor<2xi64>, %paddings: tensor<2x2xi64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{requires rank of input >= 1 + rank of block}}
  %0 = "tf.SpaceToBatchND"(%input, %block_shape, %paddings) : (tensor<3x5xf32>, tensor<2xi64>, tensor<2x2xi64>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

// Test invalid tf.SpaceToBatchND
func @testSpaceToBatchND(%input: tensor<3x5x7x10xf32>, %paddings: tensor<2x2xi64>) -> tensor<?x?x?x10xf32> {
  %block_shape = "tf.Const"() {value = dense<[1, 0]> : tensor<2xi64>} : () -> tensor<2xi64>
  // expected-error @+1 {{requires all values of block_shape to be >= 1; failed for dimension 1}}
  %1 = "tf.SpaceToBatchND"(%input, %block_shape, %paddings) : (tensor<3x5x7x10xf32>, tensor<2xi64>, tensor<2x2xi64>) -> tensor<?x?x?x10xf32>
  return %1 : tensor<?x?x?x10xf32>
}

// -----

// Test invalid tf.SpaceToBatchND
func @testSpaceToBatchND(%input: tensor<3x5x7x10xf32>, %block_shape: tensor<2xi64>) -> tensor<?x?x?x10xf32> {
  %paddings = "tf.Const"() {value = dense<[[1, 0], [-1, 0]]> : tensor<2x2xi64>} : () -> tensor<2x2xi64>
  // expected-error @+1 {{requires all values of paddings to be >= 0; failed for dimension 1}}
  %1 = "tf.SpaceToBatchND"(%input, %block_shape, %paddings) : (tensor<3x5x7x10xf32>, tensor<2xi64>, tensor<2x2xi64>) -> tensor<?x?x?x10xf32>
  return %1 : tensor<?x?x?x10xf32>
}

// -----

// Test invalid tf.SpaceToBatchND
func @testSpaceToBatchND(%input: tensor<3x5x7x10xf32>) -> tensor<36x2x3x10xf32> {
  %block_shape = "tf.Const"() {value = dense<[4, 3]> : tensor<2xi64>} : () -> tensor<2xi64>
  %paddings = "tf.Const"() {value = dense<[[1, 2], [1, 2]]> : tensor<2x2xi64>} : () -> tensor<2x2xi64>
  // expected-error @+1 {{requires block_shape[i] divides input_shape[i + 1] + paddings[i, 0] + paddings[i, 1]; failed for i=1}}
  %1 = "tf.SpaceToBatchND"(%input, %block_shape, %paddings) : (tensor<3x5x7x10xf32>, tensor<2xi64>, tensor<2x2xi64>) -> tensor<36x2x3x10xf32>
  return %1 : tensor<36x2x3x10xf32>
}

// -----

//===--------------------------------------------------------------------===//
//  tf.SparseSoftmaxCrossEntropyWithLogits
//===--------------------------------------------------------------------===//

// Test valid tf.SparseSoftmaxCrossEntropyWithLogits
// CHECK-LABEL: func @testSparseSoftmaxCrossEntropyWithLogits
func @testSparseSoftmaxCrossEntropyWithLogits(%arg0: tensor<2x3xf32>, %arg1: tensor<2xi32>) -> (tensor<3xf32>, tensor<2x3xf32>) {
  %0:2 = "tf.SparseSoftmaxCrossEntropyWithLogits"(%arg0, %arg1) : (tensor<2x3xf32>, tensor<2xi32>) -> (tensor<3xf32>, tensor<2x3xf32>)
  return %0#0, %0#1 : tensor<3xf32>, tensor<2x3xf32>
}

// -----

func @testSparseSoftmaxCrossEntropyWithLogits(%arg0: tensor<3xf32>, %arg1: tensor<3xi32>) -> (tensor<3xf32>, tensor<2x3xf32>) {
  // expected-error @+1 {{requires features operand of rank two}}
  %0:2 = "tf.SparseSoftmaxCrossEntropyWithLogits"(%arg0, %arg1) : (tensor<3xf32>, tensor<3xi32>) -> (tensor<3xf32>, tensor<2x3xf32>)
  return %0#0, %0#1 : tensor<3xf32>, tensor<2x3xf32>
}

// -----

func @testSparseSoftmaxCrossEntropyWithLogits(%arg0: tensor<2x3xf32>, %arg1: tensor<2x3xi32>) -> (tensor<2xf32>, tensor<2x3xf32>) {
  // expected-error @+1 {{requires labels operand of rank one}}
  %0:2 = "tf.SparseSoftmaxCrossEntropyWithLogits"(%arg0, %arg1) : (tensor<2x3xf32>, tensor<2x3xi32>) -> (tensor<2xf32>, tensor<2x3xf32>)
  return %0#0, %0#1 : tensor<2xf32>, tensor<2x3xf32>
}

// -----

func @testSparseSoftmaxCrossEntropyWithLogits(%arg0: tensor<2x3xf32>, %arg1: tensor<3xi32>) -> (tensor<2xf32>, tensor<2x3xf32>) {
  // expected-error @+1 {{requires features and labels with matching first dimension}}
  %0:2 = "tf.SparseSoftmaxCrossEntropyWithLogits"(%arg0, %arg1) : (tensor<2x3xf32>, tensor<3xi32>) -> (tensor<2xf32>, tensor<2x3xf32>)
  return %0#0, %0#1 : tensor<2xf32>, tensor<2x3xf32>
}

// -----

func private @testWhileCond(tensor<*xf32>) -> (tensor<i1>)
func private @testWhileBody(tensor<*xf32>) -> (tensor<*xf32>)

// Test valid 'While' operation
// CHECK-LABEL: func @testWhileResult
func @testWhileResult(tensor<*xf32>) -> (tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xf32>)

  return %1 : tensor<*xf32>
}

// -----
func @testWhileUndefinedCond(%arg0: tensor<i1>, %arg1: tensor<f32>) -> tensor<f32> {
  // expected-error @+1 {{cond refers to an undefined function : undefined_func}}
  %0 = "tf.While"(%arg0, %arg1) {cond = @undefined_func, body = @body, is_stateless = false} : (tensor<i1>, tensor<f32>) -> (tensor<f32>)
  return %0 : tensor<f32>
}

func private @body(%arg0: tensor<i1>, %arg1: tensor<f32>) -> tensor<f32>

// -----
func @testWhileUndefinedBody(%arg0: tensor<i1>, %arg1: tensor<f32>) -> tensor<f32> {
  // expected-error @+1 {{body refers to an undefined function : undefined_func}}
  %0 = "tf.While"(%arg0, %arg1) {cond = @cond, body = @undefined_func, is_stateless = false} : (tensor<i1>, tensor<f32>) -> (tensor<f32>)
  return %0 : tensor<f32>
}

func private @cond(%arg0: tensor<i1>, %arg1: tensor<f32>) -> tensor<i1>

// -----

func private @testWhileCond(tensor<*xf32>) -> ()
func private @testWhileBody(tensor<*xf32>) -> (tensor<*xf32>)

// Test invalid 'While' operation
func @testWhileResult(tensor<*xf32>) -> (tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{requires cond function to have exactly one result}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xf32>)

  return %1 : tensor<*xf32>
}

// -----

func private @testWhileCond(tensor<*xf32>) -> (tensor<i1>)
func private @testWhileBody(tensor<*xf32>) -> (tensor<*xf32>)

// Test invalid 'While' operation
func @testWhileResult(tensor<*xf32>) -> (tensor<*xi32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{'tf.While' op input type tensor<*xf32> is incompatible with result type tensor<*xi32> at index 0}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xi32>)

  return %1 : tensor<*xi32>
}

// -----

func private @testWhileCond(tensor<*xi32>) -> (tensor<i1>)
func private @testWhileBody(tensor<*xf32>) -> (tensor<*xf32>)

// Test invalid 'While' operation
func @testWhileResult(tensor<*xf32>) -> (tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{'tf.While' op input type tensor<*xf32> is incompatible with condition input type tensor<*xi32> at index 0}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xf32>)

  return %1 : tensor<*xf32>
}

// -----

func private @testWhileCond(tensor<*xf32>) -> (tensor<i1>)
func private @testWhileBody(tensor<*xf32>, tensor<*xf32>) -> (tensor<*xf32>)

// Test invalid 'While' operation
func @testWhileResult(tensor<*xf32>) -> (tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{'tf.While' op inputs (size = 1) should have the same number of values as body inputs (size = 2)}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xf32>)

  return %1 : tensor<*xf32>
}

// -----

func private @testWhileCond(tensor<*xf32>) -> (tensor<i1>)
func private @testWhileBody(tensor<*xf32>) -> (tensor<*xi32>)

// Test invalid 'While' operation
func @testWhileResult(tensor<*xf32>) -> (tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{'tf.While' op result type tensor<*xf32> is incompatible with body result type tensor<*xi32> at index 0}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xf32>)

  return %1 : tensor<*xf32>
}

// -----

func private @testWhileCond(tensor<3xf32>) -> (tensor<i1>)
func private @testWhileBody(tensor<4xf32>) -> (tensor<*xf32>)

// Test invalid 'While' operation
func @testWhileResult(tensor<*xf32>) -> (tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{'tf.While' op condition input type tensor<3xf32> is incompatible with body input type tensor<4xf32> at index 0}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*xf32>) -> (tensor<*xf32>)

  return %1 : tensor<*xf32>
}

// -----

func private @testWhileCond(tensor<*x!tf.resource<tensor<32xf32>>>) -> (tensor<i1>)
func private @testWhileBody(tensor<*x!tf.resource<tensor<32xf32>>>) -> (tensor<!tf.resource<tensor<16xf32>>>)

// Test invalid 'While' operation verifier that detects incompatible tf.resource
// subtypes.
func @testWhileResult(tensor<*x!tf.resource<tensor<32xf32>>>) -> (tensor<!tf.resource<tensor<16xf32>>>) {
^bb0(%arg0: tensor<*x!tf.resource<tensor<32xf32>>>):
  // expected-error @+1 {{'tf.While' op input type tensor<*x!tf.resource<tensor<32xf32>>> is incompatible with result type tensor<!tf.resource<tensor<16xf32>>> at index 0}}
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*x!tf.resource<tensor<32xf32>>>) -> (tensor<!tf.resource<tensor<16xf32>>>)

  return %1 : tensor<!tf.resource<tensor<16xf32>>>
}

// -----

func private @testWhileCond(tensor<*x!tf.resource<tensor<32xf32>>>) -> (tensor<i1>)
func private @testWhileBody(tensor<*x!tf.resource<tensor<32xf32>>>) -> (tensor<!tf.resource<tensor<*xf32>>>)

// Test 'While' operation verifier allows compatible tf.resource subtypes.
// CHECK-LABEL: func @testWhileResult
func @testWhileResult(tensor<*x!tf.resource<tensor<32xf32>>>) -> (tensor<!tf.resource<tensor<*xf32>>>) {
^bb0(%arg0: tensor<*x!tf.resource<tensor<32xf32>>>):
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*x!tf.resource<tensor<32xf32>>>) -> (tensor<!tf.resource<tensor<*xf32>>>)

  return %1 : tensor<!tf.resource<tensor<*xf32>>>
}

// -----

func private @testWhileCond(tensor<*x!tf.resource<tensor<32xf32>>>) -> (tensor<i1>)
func private @testWhileBody(tensor<*x!tf.resource<tensor<32xf32>>>) -> (tensor<!tf.resource>)

// Test 'While' operation verifier treats tf.resource with subtype and without
// subtype as compatible types.
// CHECK-LABEL: func @testWhileResult
func @testWhileResult(tensor<*x!tf.resource<tensor<32xf32>>>) -> (tensor<!tf.resource>) {
^bb0(%arg0: tensor<*x!tf.resource<tensor<32xf32>>>):
  %1 = "tf.While"(%arg0) {
    cond = @testWhileCond,
    body = @testWhileBody,
    is_stateless = false
  } : (tensor<*x!tf.resource<tensor<32xf32>>>) -> (tensor<!tf.resource>)

  return %1 : tensor<!tf.resource>
}

// -----

func private @cond(tensor<1x?x3xf32>) -> tensor<i1>
func private @body(tensor<1x?x3xf32>) -> tensor<1x?x3xf32>

// Test shape invariant 'While' operation verifier with different operand and
// result shapes.
// CHECK-LABEL: func @testShapeInvariantWhile
func @testShapeInvariantWhile(%arg0: tensor<1x2x3xf32>) -> tensor<1x8x3xf32> {
  %0 = "tf.While"(%arg0) {cond = @cond, body = @body, is_stateless = false, shape_invariant} : (tensor<1x2x3xf32>) -> tensor<1x8x3xf32>
  return %0 : tensor<1x8x3xf32>
}

// -----
// WhileRegion tests

// Simple While region
// CHECK-LABEL: testValidWhileRegion
func @testValidWhileRegion(%arg0 : tensor<*xf32>, %arg1 : tensor<i32>) -> tensor<*xf32> {
  %0:2 = "tf.WhileRegion"(%arg0, %arg1) (
    {
      // condition, check if count has reached 0
      ^bb0(%carg0: tensor<*xf32>, %carg1: tensor<i32>):
      %zero = constant dense<0> : tensor<i32>
      %ne = "tf.NotEqual"(%carg1, %zero) : (tensor<i32>, tensor<i32>) -> tensor<i1>
      "tf.Yield"(%ne) : (tensor<i1>) -> ()
    },
    {
      // loop body
      ^bb0(%barg0: tensor<*xf32>, %barg1: tensor<i32>):
      %add = "tf.Add"(%barg0, %barg0) : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
      %one = constant dense<1> : tensor<i32>
      %sub = "tf.Sub"(%barg1, %one) : (tensor<i32>, tensor<i32>) -> tensor<i32>
      "tf.Yield"(%add, %sub) : (tensor<*xf32>, tensor<i32>) -> ()
    }
  ) { is_stateless = false } : (tensor<*xf32>, tensor<i32>) -> (tensor<*xf32>, tensor<i32>)

  return %0#0 : tensor<*xf32>
}

// -----

// While region with no inputs (and hence no outputs) (infinite loop)
// CHECK-LABEL: testValidWhileRegionNoInputs
func private @printer(tensor<i32>) -> ()
func @testValidWhileRegionNoInputs() -> () {
  "tf.WhileRegion"() (
    {
      %true = constant dense<1> : tensor<i1>
      "tf.Yield"(%true) : (tensor<i1>) -> ()
    },
    {
      %one = constant dense<1> : tensor<i32>
      call @printer(%one) : (tensor<i32>) -> ()
      // TODO(b/159753381): tf.IfRegion implicit terminator not working
      "tf.Yield"() : () -> ()
    }
  ) { is_stateless = true } : () -> ()
  return
}

// -----
// Invalid while tests. There are 5 sets of type matching that is required
//   I = input, O = output, BI, BO = body input/output, CI = cond input.
//   [I, O], [I, CI], [I, BI], [BO, BI], [BO, O].
// Each check can fail due to number or type mismatch. However, these
// conditions are not all independent. So we just check I->{CI, BI}, O->BO, and
// in addition I->O. BO->BI mismatch cannot be independently created without
// breaking one of these mismatches. That gives us 4x2 tests. In addition
// condition result needs to be tensor<i1>, for which we have 3
// additional validation tests. All these tests are based on the following
// valid while

func @testInvalidTestValidBase(%arg0 : tensor<i32>) -> (tensor<i32>) {
  %0 = "tf.WhileRegion"(%arg0) (
    {
     ^bb0(%carg: tensor<i32>):
      %false = constant dense<false> : tensor<i1>
      "tf.Yield"(%false) : (tensor<i1>) -> ()
    },
    {
     ^bb0(%barg: tensor<i32>):
      "tf.Yield"(%barg) : (tensor<i32>) -> ()
    }
  ) { is_stateless = true } : (tensor<i32>) -> (tensor<i32>)
  return %0 : tensor<i32>
}

func @testInvalidWhileRegion_I_CI_CountMismatch(%arg0 : tensor<i32>) -> (tensor<i32>) {
  // expected-error @+1 {{'tf.WhileRegion' op inputs (size = 1) should have the same number of values as condition inputs (size = 0)}}
  %0 = "tf.WhileRegion"(%arg0) (
    {
     //^bb0(%carg: tensor<i32>):
      %false = constant dense<false> : tensor<i1>
      "tf.Yield"(%false) : (tensor<i1>) -> ()
    },
    {
     ^bb0(%barg: tensor<i32>):
      "tf.Yield"(%barg) : (tensor<i32>) -> ()
    }
  ) { is_stateless = true } : (tensor<i32>) -> (tensor<i32>)
  return %0 : tensor<i32>
}

// -----

func @testInvalidWhileRegion_I_CI_TypeMismatch(%arg0 : tensor<i32>) -> (tensor<i32>) {
  // expected-error @+1 {{'tf.WhileRegion' op input type tensor<i32> is incompatible with condition input type tensor<f32> at index 0}}
  %0 = "tf.WhileRegion"(%arg0) (
    {
     ^bb0(%carg: tensor<f32>):
      %false = constant dense<false> : tensor<i1>
      "tf.Yield"(%false) : (tensor<i1>) -> ()
    },
    {
     ^bb0(%barg: tensor<i32>):
      "tf.Yield"(%barg) : (tensor<i32>) -> ()
    }
  ) { is_stateless = true } : (tensor<i32>) -> (tensor<i32>)
  return %0 : tensor<i32>
}

// -----

func @testInvalidWhileRegion_I_BI_CountMismatch(%arg0 : tensor<i32>) -> (tensor<i32>) {
  // expected-error @+1 {{'tf.WhileRegion' op inputs (size = 1) should have the same number of values as body inputs (size = 2)}}
  %0 = "tf.WhileRegion"(%arg0) (
     {
       ^bb0(%carg: tensor<i32>):
        %true = constant dense<1> : tensor<i1>
        "tf.Yield"(%true) : (tensor<i1>) -> ()
     },
     {
       ^bb0(%barg0: tensor<i32>, %barg1 : tensor<f32>):
        "tf.Yield"(%barg0) : (tensor<i32>) -> ()
     }
  ) {is_stateless = false} : (tensor<i32>) -> (tensor<i32>)

  return %0 : tensor<i32>
}

// -----

func @testInvalidWhileRegion_I_BI_TypeMismatch(%arg0 : tensor<i32>) -> (tensor<i32>) {
  // expected-error @+1 {{'tf.WhileRegion' op input type tensor<i32> is incompatible with body input type tensor<f32> at index 0}}
  %0 = "tf.WhileRegion"(%arg0) (
     {
       ^bb0(%carg: tensor<i32>):
        %true = constant dense<1> : tensor<i1>
        "tf.Yield"(%true) : (tensor<i1>) -> ()
     },
     {
       ^bb0(%barg: tensor<f32>):
        %c = "tf.Cast"(%barg) : (tensor<f32>) -> tensor<i32>
        "tf.Yield"(%c) : (tensor<i32>) -> ()
     }
  ) {is_stateless = false} : (tensor<i32>) -> (tensor<i32>)

  return %0 : tensor<i32>
}

// -----

func @testInvalidWhileRegion_O_BO_CountMismatch(%arg0 : tensor<i32>) -> (tensor<i32>) {
  // expected-error @+1 {{'tf.WhileRegion' op results (size = 1) should have the same number of values as body results (size = 2)}}
  %0 = "tf.WhileRegion"(%arg0) (
    {
     ^bb0(%carg: tensor<i32>):
      %false = constant dense<false> : tensor<i1>
      "tf.Yield"(%false) : (tensor<i1>) -> ()
    },
    {
     ^bb0(%barg: tensor<i32>):
      "tf.Yield"(%barg, %barg) : (tensor<i32>, tensor<i32>) -> ()
    }
  ) { is_stateless = true } : (tensor<i32>) -> (tensor<i32>)
  return %0#0 : tensor<i32>
}

// -----

func @testInvalidWhileRegionMismatch_O_BO_TypeMismatch(%arg0 : tensor<i32>, %arg1: tensor<f32>) -> (tensor<i32>) {
  // expected-error @+1 {{'tf.WhileRegion' op result type tensor<i32> is incompatible with body result type tensor<f32> at index 0}}
  %0 = "tf.WhileRegion"(%arg0) (
    {
     ^bb0(%carg: tensor<i32>):
      %false = constant dense<false> : tensor<i1>
      "tf.Yield"(%false) : (tensor<i1>) -> ()
    },
    {
     ^bb0(%barg: tensor<i32>):
      "tf.Yield"(%arg1) : (tensor<f32>) -> ()
    }
  ) { is_stateless = true } : (tensor<i32>) -> (tensor<i32>)
  return %0 : tensor<i32>
}

// -----

func @testInvalidWhileRegion_I_O_CountMismatch(%arg0 : tensor<i32>) -> (tensor<i32>) {
  // expected-error@+1 {{'tf.WhileRegion' op inputs (size = 1) should have the same number of values as results (size = 2)}}
  %0:2 = "tf.WhileRegion"(%arg0) (
    {
     ^bb0(%carg: tensor<i32>):
      %false = constant dense<false> : tensor<i1>
      "tf.Yield"(%false) : (tensor<i1>) -> ()
    },
    {
     ^bb0(%barg: tensor<i32>):
      "tf.Yield"(%barg, %barg) : (tensor<i32>, tensor<i32>) -> ()
    }
  ) { is_stateless = true } : (tensor<i32>) -> (tensor<i32>, tensor<i32>)
  return %0#0 : tensor<i32>
}

// -----

func @testInvalidWhileRegion_I_O_TypeMismatch(%arg0: tensor<i32>, %arg1 : tensor<f32>) -> (tensor<f32>) {
  // expected-error@+1 {{'tf.WhileRegion' op input type tensor<i32> is incompatible with result type tensor<f32> at index 0}}
  %0 = "tf.WhileRegion"(%arg0) (
    {
     ^bb0(%carg: tensor<i32>):
      %false = constant dense<false> : tensor<i1>
      "tf.Yield"(%false) : (tensor<i1>) -> ()
    },
    {
     ^bb0(%barg: tensor<i32>):
      "tf.Yield"(%arg1) : (tensor<f32>) -> ()
    }
  ) { is_stateless = true } : (tensor<i32>) -> (tensor<f32>)
  return %0 : tensor<f32>
}
// -----

func @testInvalidWhileRegionConditionOutputCount2(%arg : tensor<i32>) -> (tensor<i32>) {
  // expected-error @+1 {{'tf.WhileRegion' op condition should have a single tensor<i1> result}}
  %0 = "tf.WhileRegion"(%arg) (
     {
       ^bb0(%carg: tensor<i32>):
        %true = constant dense<1> : tensor<i1>
        "tf.Yield"(%true, %true) : (tensor<i1>, tensor<i1>) -> ()
     },
     {
       ^bb0(%barg: tensor<i32>):
        "tf.Yield"(%barg) : (tensor<i32>) -> ()
     }
  ) {is_stateless = false} : (tensor<i32>) -> (tensor<i32>)

  return %0 : tensor<i32>
}

// -----

func @testInvalidWhileRegionConditionOutputCount0(%arg : tensor<i32>) -> (tensor<i32>) {
  // expected-error @+1 {{'tf.WhileRegion' op condition should have a single tensor<i1> result}}
  %0 = "tf.WhileRegion"(%arg) (
     {
       ^bb0(%carg: tensor<i32>):
        "tf.Yield"() : () -> ()
     },
     {
       ^bb0(%barg: tensor<i32>):
        "tf.Yield"(%barg) : (tensor<i32>) -> ()
     }
  ) {is_stateless = false} : (tensor<i32>) -> (tensor<i32>)

  return %0 : tensor<i32>
}

// -----

func @testInvalidWhileRegionConditionOutputType(%arg : tensor<i32>) -> (tensor<i32>) {
  // expected-error @+1 {{'tf.WhileRegion' op condition should have a single tensor<i1> result}}
  %0 = "tf.WhileRegion"(%arg) (
     {
       ^bb0(%carg: tensor<i32>):
        "tf.Yield"(%carg) : (tensor<i32>) -> ()
     },
     {
       ^bb0(%barg: tensor<i32>):
        "tf.Yield"(%barg) : (tensor<i32>) -> ()
     }
  ) {is_stateless = false} : (tensor<i32>) -> (tensor<i32>)

  return %0 : tensor<i32>
}

// -----

// Test shape invariant 'WhileRegion' operation verifier with different operand
// and result shapes.
// CHECK-LABEL: func @testShapeInvariantWhileRegion
func @testShapeInvariantWhileRegion(%arg0: tensor<1x2x3xf32>) -> tensor<1x8x3xf32> {
  %0 = "tf.WhileRegion"(%arg0) ( {
  ^cond(%carg0: tensor<1x?x3xf32>):
    %1 = "tf.SomeCondOp"(%carg0) : (tensor<1x?x3xf32>) -> tensor<i1>
    "tf.Yield"(%1) : (tensor<i1>) -> ()
  }, {
  ^body(%barg0: tensor<1x?x3xf32>):
    %1 = "tf.SomeBodyOp"(%barg0) : (tensor<1x?x3xf32>) -> tensor<1x?x3xf32>
    "tf.Yield"(%1) : (tensor<1x?x3xf32>) -> ()
  }) {is_stateless = false, shape_invariant} : (tensor<1x2x3xf32>) -> tensor<1x8x3xf32>
  return %0 : tensor<1x8x3xf32>
}

// -----

// CHECK-LABEL: func @testValidShape
func @testValidShape(tensor<1x32x32x16xf32>, tensor<*xf32>) -> (tensor<4xi32>, tensor<?xi32>) {
^bb0(%arg0: tensor<1x32x32x16xf32>, %arg1: tensor<*xf32>):
  %0 = "tf.Shape"(%arg0) {T = "tfdtype$DT_FLOAT", output = "tfdtype$DT_INT32"} : (tensor<1x32x32x16xf32>) -> tensor<4xi32>
  %1 = "tf.Shape"(%arg1) {T = "tfdtype$DT_FLOAT", output = "tfdtype$DT_INT32"} : (tensor<*xf32>) -> tensor<?xi32>
  return %0, %1 : tensor<4xi32>, tensor<?xi32>
}

// -----

func @testShapeWrongResultElemType(%arg0: tensor<1x32x32x16xf32>) -> tensor<4xf32> {
  // expected-error @+1 {{result #0 must be tensor of 32/64-bit signed integer values}}
  %0 = "tf.Shape"(%arg0) : (tensor<1x32x32x16xf32>) -> tensor<4xf32>
  return %0 : tensor<4xf32>
}

// -----

func @testShapeWrongResultDim(tensor<1x32x32x16xf32>) -> tensor<3x2xi32> {
^bb0(%arg0: tensor<1x32x32x16xf32>):
  // expected-error @+1 {{requires 1D type for result}}
  %0 = "tf.Shape"(%arg0) {T = "tfdtype$DT_FLOAT", output = "tfdtype$DT_INT32"} : (tensor<1x32x32x16xf32>) -> tensor<3x2xi32>
  return %0 : tensor<3x2xi32>
}

// -----

func @testShapeMismatchDim(tensor<1x32x32x16xf32>) -> tensor<2xi32> {
^bb0(%arg0: tensor<1x32x32x16xf32>):
  // expected-error @+1 {{requires dimension size of result to match rank of operand}}
  %0 = "tf.Shape"(%arg0) {T = "tfdtype$DT_FLOAT", output = "tfdtype$DT_INT32"} : (tensor<1x32x32x16xf32>) -> tensor<2xi32>
  return %0 : tensor<2xi32>
}

// -----

func @testShapeWrongResultDimDynamic(tensor<*xf32>) -> tensor<2xi32> {
^bb0(%arg0: tensor<*xf32>):
  // expected-warning @+1 {{has static shape result for unranked operand}}
  %0 = "tf.Shape"(%arg0) {T = "tfdtype$DT_FLOAT", output = "tfdtype$DT_INT32"} : (tensor<*xf32>) -> tensor<2xi32>
  return %0 : tensor<2xi32>
}

// -----

// CHECK-LABEL: func @testValidShapeN
func @testValidShapeN(%arg0 : tensor<1x32x32x16xf32>, %arg1 : tensor<*xf32>) -> (tensor<4xi32>, tensor<?xi32>) {
  // CHECK-NEXT: "tf.ShapeN"
  %0:2 = "tf.ShapeN"(%arg0, %arg1) : (tensor<1x32x32x16xf32>, tensor<*xf32>) -> (tensor<4xi32>, tensor<?xi32>)
  return %0#0, %0#1 : tensor<4xi32>, tensor<?xi32>
}

// -----

func @testShapeNWrongResultElemType(%arg0: tensor<1x32x32x16xf32>) -> tensor<4xf32> {
  // expected-error @+1 {{result #1 must be tensor of 32/64-bit signed integer values}}
  %0:2 = "tf.ShapeN"(%arg0, %arg0) : (tensor<1x32x32x16xf32>, tensor<1x32x32x16xf32>) -> (tensor<4xi32>, tensor<4xf32>)
  return %0#1 : tensor<4xf32>
}

// -----

func @testShapeNWrongResultDim(tensor<1x32x32x16xf32>) -> tensor<2x2xi32> {
^bb0(%arg0: tensor<1x32x32x16xf32>):
  // expected-error @+1 {{requires 1D type for result #1}}
  %0:2 = "tf.ShapeN"(%arg0, %arg0) : (tensor<1x32x32x16xf32>, tensor<1x32x32x16xf32>) -> (tensor<4xi32>, tensor<2x2xi32>)
  return %0#1 : tensor<2x2xi32>
}

// -----

func @testShapeNMismatchDim(tensor<1x32x32x16xf32>) -> tensor<2xi32> {
^bb0(%arg0: tensor<1x32x32x16xf32>):
  // expected-error @+1 {{requires dimension size of result #1 to match rank of operand #1}}
  %0:2 = "tf.ShapeN"(%arg0, %arg0) : (tensor<1x32x32x16xf32>, tensor<1x32x32x16xf32>) -> (tensor<4xi32>, tensor<2xi32>)
  return %0#1 : tensor<2xi32>
}

// -----

func @testShapeNWrongResultDimDynamic(tensor<*xf32>) -> tensor<2xi32> {
^bb0(%arg0: tensor<*xf32>):
  // expected-warning @+1 {{has static shape result #1 for unranked operand #1}}
  %0:2 = "tf.ShapeN"(%arg0, %arg0) : (tensor<*xf32>, tensor<*xf32>) -> (tensor<?xi32>, tensor<2xi32>)
  return %0#1 : tensor<2xi32>
}

// -----

func @testShapeNWrongNumResults(tensor<*xf32>) {
^bb0(%arg0: tensor<*xf32>):
  // expected-error @+1 {{requires 3 result(s), got 2 result(s)}}
  %0:2 = "tf.ShapeN"(%arg0, %arg0, %arg0) : (tensor<*xf32>, tensor<*xf32>, tensor<*xf32>) -> (tensor<?xi32>, tensor<?xi32>)
  return
}

// -----

// CHECK-LABEL: func @testValidVariableShape
func @testValidVariableShape(%arg0: tensor<*x!tf.resource<tensor<1x32x32x16xf32>>>, %arg1: tensor<*x!tf.resource>) -> (tensor<4xi32>, tensor<?xi32>) {
  %0 = "tf.VariableShape"(%arg0) {output = "tfdtype$DT_INT32"} : (tensor<*x!tf.resource<tensor<1x32x32x16xf32>>>) -> tensor<4xi32>
  %1 = "tf.VariableShape"(%arg1) {output = "tfdtype$DT_INT32"} : (tensor<*x!tf.resource>) -> tensor<?xi32>
  return %0, %1 : tensor<4xi32>, tensor<?xi32>
}

// -----

func @testVariableShapeMultipleSubtypes(%arg0: tensor<*x!tf.resource<tensor<1x32x32x16xf32>, tensor<1x32x32x16xf32>>>) {
  // expected-error @+1 {{requires resource input type to have at most 1 subtype}}
  %0 = "tf.VariableShape"(%arg0) {output = "tfdtype$DT_INT32"} : (tensor<*x!tf.resource<tensor<1x32x32x16xf32>, tensor<1x32x32x16xf32>>>) -> tensor<4xi32>
  return
}

// -----

func @testVariableShapeWrongResultElemType(%arg0: tensor<*x!tf.resource<tensor<1x32x32x16xf32>>>) -> tensor<?xf32> {
  // expected-error @+1 {{result #0 must be tensor of 32/64-bit signed integer values}}
  %0 = "tf.VariableShape"(%arg0) : (tensor<*x!tf.resource<tensor<1x32x32x16xf32>>>) -> tensor<4xf32>
  return %0 : tensor<4xf32>
}

// -----

func @testVariableShapeWrongResultDim(%arg0: tensor<*x!tf.resource<tensor<1x32x32x16xf32>>>) -> tensor<2x3xi32> {
  // expected-error @+1 {{requires 1D type for result}}
  %0 = "tf.VariableShape"(%arg0) {output = "tfdtype$DT_INT32"} : (tensor<*x!tf.resource<tensor<1x32x32x16xf32>>>) -> tensor<2x3xi32>
  return %0 : tensor<2x3xi32>
}

// -----

func @testVariableShapeMismatchDim(%arg0: tensor<*x!tf.resource<tensor<1x32x32x16xf32>>>) -> tensor<2xi32> {
  // expected-error @+1 {{requires dimension size of result to match rank of operand}}
  %0 = "tf.VariableShape"(%arg0) {output = "tfdtype$DT_INT32"} : (tensor<*x!tf.resource<tensor<1x32x32x16xf32>>>) -> tensor<2xi32>
  return %0 : tensor<2xi32>
}

// -----

func @testVariableShapeWrongResultDimDynamic(%arg0: tensor<*x!tf.resource<tensor<*xf32>>>) -> tensor<2xi32> {
  // expected-warning @+1 {{has static shape result for unranked operand}}
  %0 = "tf.VariableShape"(%arg0) {output = "tfdtype$DT_INT32"} : (tensor<*x!tf.resource<tensor<*xf32>>>) -> tensor<2xi32>
  return %0 : tensor<2xi32>
}

// -----

func @testVariableShapeWrongNumResources(%arg0: tensor<1x2x!tf.resource<tensor<1x32x32x16xf32>>>) -> tensor<4xi32> {
  // expected-error @+1 {{requires input to have one resource}}
  %0 = "tf.VariableShape"(%arg0)  : (tensor<1x2x!tf.resource<tensor<1x32x32x16xf32>>>) -> tensor<4xi32>
  return %0 : tensor<4xi32>
}

// -----

// Test invalid tf.Const
func @testConst() -> tensor<f32> {
  // expected-error @+1 {{attribute 'value' failed to satisfy constraint: constant vector/tensor}}
  %0 = "tf.Const"() {T = "tfdtype$DT_FLOAT", value = 1.0 : f32} : () -> tensor<f32>
  return %0 : tensor<f32>
}

// -----

// Test invalid tf.ToBool
func @testInvalidToBool(%arg0: tensor<i32>) -> tensor<1xi1> {
  // expected-error @+1 {{op result #0 must be 0D tensor of 1-bit signless integer values, but got 'tensor<1xi1>'}}
  %0 = "tf.ToBool"(%arg0) : (tensor<i32>) -> tensor<1xi1>
  return %0 : tensor<1xi1>
}

// -----

// Test valid tf.Transpose
// CHECK-LABEL: testTranspose
func @testTranspose(tensor<2x3xf32>) -> tensor<3x2xf32> {
^bb0(%arg0: tensor<2x3xf32>):
  %cst = constant dense<[1, 0]> : tensor<2xi32>
  %0 = "tf.Transpose"(%arg0, %cst) {T = "tfdtype$DT_FLOAT", Tperm = "tfdtype$DT_INT32"} : (tensor<2x3xf32>, tensor<2xi32>) -> tensor<3x2xf32>
  return %0 : tensor<3x2xf32>
}

// -----

// Test tf.Transpose with partial unknown shape
// CHECK-LABEL: testTranspose
func @testTranspose(tensor<2x?xf32>) -> tensor<?x2xf32> {
^bb0(%arg0: tensor<2x?xf32>):
  %cst = constant dense<[1, 0]> : tensor<2xi32>
  %0 = "tf.Transpose"(%arg0, %cst) {T = "tfdtype$DT_FLOAT", Tperm = "tfdtype$DT_INT32"} : (tensor<2x?xf32>, tensor<2xi32>) -> tensor<?x2xf32>
  return %0 : tensor<?x2xf32>
}

// -----

// Test tf.Transpose with different partial unknown shape
// CHECK-LABEL: testTranspose
func @testTranspose(tensor<2x?x?xf32>) -> tensor<3x?x2xf32> {
^bb0(%arg0: tensor<2x?x?xf32>):
  %cst = constant dense<[2, 1, 0]> : tensor<3xi32>
  %0 = "tf.Transpose"(%arg0, %cst) {T = "tfdtype$DT_FLOAT", Tperm = "tfdtype$DT_INT32"} : (tensor<2x?x?xf32>, tensor<3xi32>) -> tensor<3x?x2xf32>
  return %0 : tensor<3x?x2xf32>
}

// -----

// Test tf.Transpose with invalid rank of perm
func @testTranspose(tensor<2x3xf32>, tensor<1x2xi32>) -> tensor<3x2xf32> {
^bb0(%arg0: tensor<2x3xf32>, %arg1: tensor<1x2xi32>):
  // expected-error @+1 {{expected perm to be a 1-D Tensor, got perm of rank 2}}
  %0 = "tf.Transpose"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", Tperm = "tfdtype$DT_INT32"} : (tensor<2x3xf32>, tensor<1x2xi32>) -> tensor<3x2xf32>
  return %0 : tensor<3x2xf32>
}

// -----

// Test tf.Transpose with invalid size of perm
func @testTranspose(tensor<2x3xf32>) -> tensor<3x2xf32> {
^bb0(%arg0: tensor<2x3xf32>):
  %cst = constant dense<[1, 0, 2]> : tensor<3xi32>
  // expected-error @+1 {{expected perm to be a 1-D Tensor of size equal to the rank of x, got perm of size 3, and x of rank 2}}
  %0 = "tf.Transpose"(%arg0, %cst) {T = "tfdtype$DT_FLOAT", Tperm = "tfdtype$DT_INT32"} : (tensor<2x3xf32>, tensor<3xi32>) -> tensor<3x2xf32>
  return %0 : tensor<3x2xf32>
}

// -----

// Test tf.Transpose with invalid rank of y
func @testTranspose(tensor<2x3xf32>) -> tensor<3x2x1xf32> {
^bb0(%arg0: tensor<2x3xf32>):
  %cst = constant dense<[1, 0]> : tensor<2xi32>
  // expected-error @+1 {{x should be of the same rank with y, got x of rank 2, and y of rank 3}}
  %0 = "tf.Transpose"(%arg0, %cst) {T = "tfdtype$DT_FLOAT", Tperm = "tfdtype$DT_INT32"} : (tensor<2x3xf32>, tensor<2xi32>) -> tensor<3x2x1xf32>
  return %0 : tensor<3x2x1xf32>
}

// -----

// Test tf.Transpose with invalid shape of y
func @testTranspose(tensor<2x3x4xf32>) -> tensor<3x2x4xf32> {
^bb0(%arg0: tensor<2x3x4xf32>):
  %cst = constant dense<[2, 0, 1]> : tensor<3xi32>
  // expected-error @+1 {{requires y.shape[0] (3) to be equal to x.shape[perm[2]] (4)}}
  %0 = "tf.Transpose"(%arg0, %cst) {T = "tfdtype$DT_FLOAT", Tperm = "tfdtype$DT_INT32"} : (tensor<2x3x4xf32>, tensor<3xi32>) -> tensor<3x2x4xf32>
  return %0 : tensor<3x2x4xf32>
}

// -----

// Test invalid tf.Less
func @testLess(tensor<4xi32>, tensor<4xi32>) -> tensor<4xi32> {
^bb0(%arg0: tensor<4xi32>, %arg1: tensor<4xi32>):
  // expected-error @+1 {{op result #0 must be tensor of bool values}}
  %0 = "tf.Less"(%arg0, %arg1) : (tensor<4xi32>, tensor<4xi32>) -> tensor<4xi32>
  return %0 : tensor<4xi32>
}

// -----

// Test valid tf.ConcatV2
func @testConcatV2(%arg: tensor<8x16xf32>, %axis: tensor<i32>) -> tensor<?xf32> {
  %0 = "tf.ConcatV2"(%arg, %arg, %axis) : (tensor<8x16xf32>, tensor<8x16xf32>, tensor<i32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
}

// -----

// tf.ConcatV2 with wrong 'axis' element type
func @testConcatV2(%arg: tensor<8x16xf32>, %axis: tensor<f32>) -> tensor<?xf32> {
  // expected-error @+1 {{operand #2 must be tensor of 32/64-bit signed integer values}}
  %0 = "tf.ConcatV2"(%arg, %arg, %axis) : (tensor<8x16xf32>, tensor<8x16xf32>, tensor<f32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
}

// -----

// tf.ConcatV2 missing required 'axis' operand
func @testConcatV2() -> tensor<?xf32> {
  // expected-error @+1 {{expected 1 or more operands}}
  %0 = "tf.ConcatV2"() : () -> tensor<?xf32>
  return %0 : tensor<?xf32>
}

// -----

// CHECK-LABEL: testAll
func @testAll(%arg0: tensor<2x2xi1>, %arg1: tensor<i32>) -> tensor<i1> {
  %0 = "tf.All"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xi1>, tensor<i32>) -> tensor<i1>
  return %0 : tensor<i1>
}

// -----

// CHECK-LABEL: testAll64
func @testAll64(%arg0: tensor<2x2xi1>, %arg1: tensor<i64>) -> tensor<i1> {
  %0 = "tf.All"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xi1>, tensor<i64>) -> tensor<i1>
  return %0 : tensor<i1>
}

// -----

func @testAllFloat(%arg0: tensor<2x2xi1>, %arg1: tensor<f32>) -> tensor<i1> {
  // expected-error @+1 {{'tf.All' op operand #1 must be tensor of 32/64-bit signed integer values}}
  %0 = "tf.All"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xi1>, tensor<f32>) -> tensor<i1>
  return %0 : tensor<i1>
}

// -----

func @testAllI32(%arg0: tensor<2x2xi32>, %arg1: tensor<f32>) -> tensor<i32> {
  // expected-error @+1 {{'tf.All' op operand #0 must be tensor of bool values}}
  %0 = "tf.All"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xi32>, tensor<f32>) -> tensor<i32>
  return %0 : tensor<i32>
}

// -----

func @testEqualOpIncompatibleShapeTrue(%x: tensor<5xf32>, %y: tensor<4xf32>) -> tensor<5xi1> {
  // expected-error @+1 {{operands don't have broadcast-compatible shapes}}
  %0 = "tf.Equal"(%x, %y) {incompatible_shape_error = true} : (tensor<5xf32>, tensor<4xf32>) -> tensor<5xi1>
  return %0 : tensor<5xi1>
}

// -----

// CHECK-LABEL: testEqualOpIncompatibleShapeFalse
func @testEqualOpIncompatibleShapeFalse(%x: tensor<5xf32>, %y: tensor<4xf32>) -> tensor<*xi1> {
  %0 = "tf.Equal"(%x, %y) {incompatible_shape_error = false} : (tensor<5xf32>, tensor<4xf32>) -> tensor<*xi1>
  return %0 : tensor<*xi1>
}

// -----

func @testNotEqualOpIncompatibleShapeTrue(%x: tensor<5xf32>, %y: tensor<4xf32>) -> tensor<5xi1> {
  // expected-error @+1 {{operands don't have broadcast-compatible shapes}}
  %0 = "tf.NotEqual"(%x, %y) {incompatible_shape_error = true} : (tensor<5xf32>, tensor<4xf32>) -> tensor<5xi1>
  return %0 : tensor<5xi1>
}

// -----

// CHECK-LABEL: testNotEqualOpIncompatibleShapeFalse
func @testNotEqualOpIncompatibleShapeFalse(%x: tensor<5xf32>, %y: tensor<4xf32>) -> tensor<*xi1> {
  %0 = "tf.NotEqual"(%x, %y) {incompatible_shape_error = false} : (tensor<5xf32>, tensor<4xf32>) -> tensor<*xi1>
  return %0 : tensor<*xi1>
}

// -----

func @testConcatV2(%arg: tensor<8x16xf32>, %axis: tensor<1x1xi32>) -> tensor<*xf32> { // expected-error @+1 {{requires axis to be of scalar type (or vector type for older versions)}}
  %0 = "tf.ConcatV2"(%arg, %arg, %axis) : (tensor<8x16xf32>, tensor<8x16xf32>, tensor<1x1xi32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @testConcatV2(%arg: tensor<8x16xf32>, %axis: tensor<1x1xi32>) -> tensor<*xf32> {
  // expected-error @+1 {{requires axis to be of scalar type (or vector type for older versions)}}
  %0 = "tf.Concat"(%axis, %arg, %arg) : (tensor<1x1xi32>, tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @testConcatV2(%arg0: tensor<8x16xf32>, %arg1: tensor<8xf32>, %axis: tensor<i32>) -> tensor<*xf32> {
  // expected-error @+1 {{operand type 'tensor<8xf32>' is not compatible with preceding operands; expected rank: 2}}
  %0 = "tf.ConcatV2"(%arg0, %arg1, %axis) : (tensor<8x16xf32>, tensor<8xf32>, tensor<i32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

// Valid Concat operation with concat axis 1 or -1.
func @testConcatV2(%arg0: tensor<8x16xf32>, %arg1: tensor<8x8xf32>, %axis: tensor<i32>) -> tensor<*xf32> {
  %0 = "tf.ConcatV2"(%arg0, %arg1, %axis) : (tensor<8x16xf32>, tensor<8x8xf32>, tensor<i32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @testConcatV2(%arg0: tensor<8x16xf32>, %arg1: tensor<16x8xf32>, %axis: tensor<i32>) -> tensor<*xf32> {
  // expected-error @+1 {{operand type 'tensor<16x8xf32>' is not compatible with preceding operands; expected dimension at index 1: 16}}
  %0 = "tf.ConcatV2"(%arg0, %arg1, %axis) : (tensor<8x16xf32>, tensor<16x8xf32>, tensor<i32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

// Valid Concat operation with concat axis 1 or -1.
func @testConcatV2(%arg0: tensor<8x8xf32>, %arg1: tensor<?x4xf32>, %arg2: tensor<*xf32>, %arg3: tensor<8x?xf32>, %axis: tensor<i32>) -> tensor<*xf32> {
  %0 = "tf.ConcatV2"(%arg0, %arg1, %arg2, %arg3, %axis) : (tensor<8x8xf32>, tensor<?x4xf32>, tensor<*xf32>, tensor<8x?xf32>, tensor<i32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @testInvalidInvertPermutationOp(%arg0: tensor<8x8xi32>) -> tensor<8x8xi32> {
  // expected-error @+1 {{'tf.InvertPermutation' op requires input x to be 1-dimensional}}
  %0 = "tf.InvertPermutation"(%arg0) : (tensor<8x8xi32>) -> tensor<8x8xi32>
  return %0 : tensor<8x8xi32>
}

// -----

// Valid Pack operation.
func @testPack(%arg0: tensor<4x8xf32>, %arg1: tensor<4x8xf32>) -> tensor<*xf32> {
  %0 = "tf.Pack"(%arg0, %arg1) {axis = 1 : i64} : (tensor<4x8xf32>, tensor<4x8xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}


// -----

func @testPack(%arg0: tensor<4x8xf32>, %arg1: tensor<4x2xf32>) -> tensor<*xf32> {
  // expected-error @+1 {{operand type 'tensor<4x2xf32>' is not compatible with preceding operands; expected dimension at index 1: 8}}
  %0 = "tf.Pack"(%arg0, %arg1) {axis = 1 : i64} : (tensor<4x8xf32>, tensor<4x2xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @testPack(%arg0: tensor<4x8xf32>, %arg1: tensor<4x8xf32>, %axis: tensor<i32>) -> tensor<*xf32> {
  // expected-error @+1 {{attribute 'axis' should be within range [-3, 3); actual value: 3}}
  %0 = "tf.Pack"(%arg0, %arg1) {axis = 3 : i64} : (tensor<4x8xf32>, tensor<4x8xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

// Valid slice operation.
func @testSlice(%arg0: tensor<3x4xi32>, %arg1: tensor<2xi64>) -> tensor<1x4xi32> {
  %sizes = "tf.Const"() {value = dense<[1, 4]> : tensor<2xi64>} : () -> (tensor<2xi64>)
  %0 = "tf.Slice"(%arg0, %arg1, %sizes) : (tensor<3x4xi32>, tensor<2xi64>, tensor<2xi64>) -> tensor<1x4xi32>
  return %0 : tensor<1x4xi32>
}

// -----

func @testSlice_begin_2d(%arg0: tensor<4xi32>, %begins: tensor<2x2xi64>) -> tensor<3xi32> {
  %sizes = "tf.Const"() {value = dense<[1]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  // expected-error @+1 {{requires begin operand to be 1D tensor}}
  %0 = "tf.Slice"(%arg0, %begins, %sizes) : (tensor<4xi32>, tensor<2x2xi64>, tensor<1xi64>) -> tensor<3xi32>
  return %0 : tensor<3xi32>
}

// -----

func @testSlice_size_two_much_elements(%arg0: tensor<4xi32>) -> tensor<3xi32> {
  %begins = "tf.Const"() {value = dense<[1]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  %sizes = "tf.Const"() {value = dense<[1, 2]> : tensor<2xi64>} : () -> (tensor<2xi64>)
  // expected-error @+1 {{requires begin and size operands to have the same number of elements}}
  %0 = "tf.Slice"(%arg0, %begins, %sizes) : (tensor<4xi32>, tensor<1xi64>, tensor<2xi64>) -> tensor<3xi32>
  return %0 : tensor<3xi32>
}

// -----

func @testSlice_begin_negative(%arg0: tensor<4xi32>) -> tensor<2xi32> {
  %begins = "tf.Const"() {value = dense<[-1]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  %sizes = "tf.Const"() {value = dense<[2]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  // expected-error @+1 {{requires 0 <= begin[i] <= begin[i] + size[i] <= Di}}
  %0 = "tf.Slice"(%arg0, %begins, %sizes) : (tensor<4xi32>, tensor<1xi64>, tensor<1xi64>) -> tensor<2xi32>
  return %0 : tensor<2xi32>
}

// -----

func @testSlice_begin_out_of_bound(%arg0: tensor<4xi32>) -> tensor<2xi32> {
  %begins = "tf.Const"() {value = dense<[4]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  %sizes = "tf.Const"() {value = dense<[2]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  // expected-error @+1 {{requires 0 <= begin[i] <= begin[i] + size[i] <= Di}}
  %0 = "tf.Slice"(%arg0, %begins, %sizes) : (tensor<4xi32>, tensor<1xi64>, tensor<1xi64>) -> tensor<2xi32>
  return %0 : tensor<2xi32>
}

// -----

func @testSlice_unknown_begin_out_of_bounds(%arg0: tensor<4xi32>, %begins: tensor<1xi64>) -> tensor<3xi32> {
  %sizes = "tf.Const"() {value = dense<[5]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  // expected-error @+1 {{requires size[i] <= Di, even if begin[i] is unknown at compile time}}
  %0 = "tf.Slice"(%arg0, %begins, %sizes) : (tensor<4xi32>, tensor<1xi64>, tensor<1xi64>) -> tensor<3xi32>
  return %0 : tensor<3xi32>
}

// -----

func @testSlice_unknown_begin_in_bounds(%arg0: tensor<4xi32>, %begins: tensor<1xi64>) -> tensor<3xi32> {
  %sizes = "tf.Const"() {value = dense<[4]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  %0 = "tf.Slice"(%arg0, %begins, %sizes) : (tensor<4xi32>, tensor<1xi64>, tensor<1xi64>) -> tensor<3xi32>
  return %0 : tensor<3xi32>
}

// -----

func @testSlice_unequal_output_input_rank(%arg0: tensor<4xi32>, %begins: tensor<1xi64>) -> tensor<i32> {
  %sizes = "tf.Const"() {value = dense<[1]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  // expected-error @+1 {{requires output to have the same rank as input, but got input rank 1 and output rank 0}}
  %0 = "tf.Slice"(%arg0, %begins, %sizes) : (tensor<4xi32>, tensor<1xi64>, tensor<1xi64>) -> tensor<i32>
  return %0 : tensor<i32>
}

// -----

func @testSlice_wrong_output_size(%arg0: tensor<4xi32>) -> tensor<1xi32> {
  %begins = "tf.Const"() {value = dense<[1]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  %sizes = "tf.Const"() {value = dense<[2]> : tensor<1xi64>} : () -> (tensor<1xi64>)
  // expected-error @+1 {{requires output size to have the same size of slice, got slice size 2 and output size 1}}
  %0 = "tf.Slice"(%arg0, %begins, %sizes) : (tensor<4xi32>, tensor<1xi64>, tensor<1xi64>) -> tensor<1xi32>
  return %0 : tensor<1xi32>
}

// -----

// Valid StridedSlice operation.
func @testStridedSlice(%input: tensor<4x8xf32>, %begin: tensor<2xi64>, %end: tensor<2xi64>, %strides: tensor<2xi64>) -> tensor<?x?xf32> {
  %0 = "tf.StridedSlice"(%input, %begin, %end, %strides) : (tensor<4x8xf32>, tensor<2xi64>, tensor<2xi64>, tensor<2xi64>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @testStridedSlice(%input: tensor<4x8xf32>, %begin: tensor<i64>, %end: tensor<i64>, %strides: tensor<i64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{requires begin, end and strides to be 1D tensors}}
  %0 = "tf.StridedSlice"(%input, %begin, %end, %strides) : (tensor<4x8xf32>, tensor<i64>, tensor<i64>, tensor<i64>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @testStridedSlice(%input: tensor<4x8xf32>, %begin: tensor<32xi64>, %end: tensor<2xi64>, %strides: tensor<2xi64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{with less than 32 elements}}
  %0 = "tf.StridedSlice"(%input, %begin, %end, %strides) : (tensor<4x8xf32>, tensor<32xi64>, tensor<2xi64>, tensor<2xi64>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @testStridedSlice(%input: tensor<4x8xf32>, %begin: tensor<?xi64>, %end: tensor<3xi64>, %strides: tensor<2xi64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{to have the same number of elements}}
  %0 = "tf.StridedSlice"(%input, %begin, %end, %strides) : (tensor<4x8xf32>, tensor<?xi64>, tensor<3xi64>, tensor<2xi64>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @testStridedSlice(%input: tensor<4x8xf32>) -> tensor<?x?xf32> {
  %begin = "tf.Const"() { value = dense<[0, 0]> : tensor<2xi64> } : () -> tensor<?xi64>
  %end = "tf.Const"() { value = dense<[5, 10]> : tensor<2xi64> } : () -> tensor<?xi64>
  %strides = "tf.Const"() { value = dense<[2, 3, 4]> : tensor<3xi64> } : () -> tensor<?xi64>

  // expected-error @+1 {{to have the same number of elements}}
  %1 = "tf.StridedSlice"(%input, %begin, %end, %strides) : (tensor<4x8xf32>, tensor<?xi64>, tensor<?xi64>, tensor<?xi64>) -> tensor<?x?xf32>
}

// -----

func @testStridedSlice(%input: tensor<4x8xf32>, %begin: tensor<2xi32>, %end: tensor<2xi32>) -> tensor<?x?xf32> {
  %strides = "tf.Const"() { value = dense<[2, 0]> : tensor<2xi32> } : () -> tensor<2xi32>

  // expected-error @+1 {{requires non-zero strides}}
  %1 = "tf.StridedSlice"(%input, %begin, %end, %strides) : (tensor<4x8xf32>, tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<?x?xf32>
  return %1 : tensor<?x?xf32>
}

// -----

func @testStridedSlice(%input: tensor<4x8xf32>, %begin: tensor<2xi64>, %end: tensor<2xi64>, %strides: tensor<2xi64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{cannot have multiple ellipses}}
  %0 = "tf.StridedSlice"(%input, %begin, %end, %strides) {ellipsis_mask = 3}: (tensor<4x8xf32>, tensor<2xi64>, tensor<2xi64>, tensor<2xi64>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @testOneHot(%indices: tensor<3xi32>, %depth: tensor<i32>, %on_value: tensor<f32>, %off_value: tensor<f32>) -> tensor<3x5xf32> {
  %result = "tf.OneHot"(%indices, %depth, %on_value, %off_value) {axis = -1 : i64} : (tensor<3xi32>, tensor<i32>, tensor<f32>, tensor<f32>) -> tensor<3x5xf32>
  return %result : tensor<3x5xf32>
}

// -----

func @testOneHot(%indices: tensor<3xi32>, %on_value: tensor<f32>, %off_value: tensor<f32>) -> tensor<3x5xf32> {
  %depth = "tf.Const"() { value = dense<-5> : tensor<i32> } : () -> tensor<i32>
  // expected-error @+1 {{depth must be non-negative}}
  %result = "tf.OneHot"(%indices, %depth, %on_value, %off_value) {axis = -1 : i64} : (tensor<3xi32>, tensor<i32>, tensor<f32>, tensor<f32>) -> tensor<3x5xf32>
  return %result : tensor<3x5xf32>
}

// -----

func @testOneHot(%indices: tensor<3xi32>, %depth: tensor<2xi32>, %on_value: tensor<f32>, %off_value: tensor<f32>) -> tensor<3x5xf32> {
  // expected-error @+1 {{requires depth to be a scalar}}
  %result = "tf.OneHot"(%indices, %depth, %on_value, %off_value) {axis = -1 : i64} : (tensor<3xi32>, tensor<2xi32>, tensor<f32>, tensor<f32>) -> tensor<3x5xf32>
  return %result : tensor<3x5xf32>
}

// -----

func @testOneHot(%indices: tensor<3xi32>, %depth: tensor<i32>, %on_value: tensor<2xf32>, %off_value: tensor<f32>) -> tensor<3x5xf32> {
  // expected-error @+1 {{requires on_value to be a scalar}}
  %result = "tf.OneHot"(%indices, %depth, %on_value, %off_value) {axis = -1 : i64} : (tensor<3xi32>, tensor<i32>, tensor<2xf32>, tensor<f32>) -> tensor<3x5xf32>
  return %result : tensor<3x5xf32>
}

// -----

func @testOneHot(%indices: tensor<3xi32>, %depth: tensor<i32>, %on_value: tensor<f32>, %off_value: tensor<2xf32>) -> tensor<3x5xf32> {
  // expected-error @+1 {{requires off_value to be a scalar}}
  %result = "tf.OneHot"(%indices, %depth, %on_value, %off_value) {axis = -1 : i64} : (tensor<3xi32>, tensor<i32>, tensor<f32>, tensor<2xf32>) -> tensor<3x5xf32>
  return %result : tensor<3x5xf32>
}

// -----

func @testOneHot(%indices: tensor<3xi32>, %depth: tensor<i32>, %on_value: tensor<f32>, %off_value: tensor<f32>) -> tensor<3x5xf32> {
  // expected-error @+1 {{expected axis (-2) to be -1 or between [0, 1]}}
  %result = "tf.OneHot"(%indices, %depth, %on_value, %off_value) {axis = -2 : i64} : (tensor<3xi32>, tensor<i32>, tensor<f32>, tensor<f32>) -> tensor<3x5xf32>
  return %result : tensor<3x5xf32>
}

// -----

func @testSplitNonConstSplitDim(%input: tensor<4x4xf32>, %split_dim: tensor<i32>) {
  %0:2 = "tf.Split"(%split_dim, %input) : (tensor<i32>, tensor<4x4xf32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

func @testSplitUnknownRankSplitDim(%input: tensor<4x4xf32>, %split_dim: tensor<*xi32>) {
  %0:2 = "tf.Split"(%split_dim, %input) : (tensor<*xi32>, tensor<4x4xf32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

func @testSplitUnknownRankInput(%input: tensor<*xf32>) {
  %cst = "tf.Const"() {value = dense<1> : tensor<i32>} : () -> tensor<i32>
  %0:2 = "tf.Split"(%cst, %input) : (tensor<i32>, tensor<*xf32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

func @testSplitUnknownDimInput(%input: tensor<4x?x4xf32>) {
  %cst = "tf.Const"() {value = dense<1> : tensor<i32>} : () -> tensor<i32>
  %0:2 = "tf.Split"(%cst, %input) : (tensor<i32>, tensor<4x?x4xf32>) -> (tensor<4x?x4xf32>, tensor<4x?x4xf32>)
  return
}

// -----

func @testSplitNonScalarSplitDim(%input: tensor<4x4xf32>, %split_dim: tensor<1xi32>) {
  // expected-error @+1 {{split dimension should be an integer scalar tensor}}
  %0:2 = "tf.Split"(%split_dim, %input) : (tensor<1xi32>, tensor<4x4xf32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testSplitScalarInput(%input: tensor<f32>, %split_dim: tensor<i32>) {
  // expected-error @+1 {{cannot split scalar input tensor}}
  %0:2 = "tf.Split"(%split_dim, %input) : (tensor<i32>, tensor<f32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testSplitLargeSplitDim(%input: tensor<4x8xf32>) {
  %cst = "tf.Const"() {value = dense<2> : tensor<i32>} : () -> tensor<i32>
  // expected-error @+1 {{split dimension must be in range [-2, 2)}}
  %0:2 = "tf.Split"(%cst, %input) : (tensor<i32>, tensor<4x8xf32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testSplitSmallSplitDim(%input: tensor<4x8xf32>) {
  %cst = "tf.Const"() {value = dense<-3> : tensor<i32>} : () -> tensor<i32>
  // expected-error @+1 {{split dimension must be in range [-2, 2)}}
  %0:2 = "tf.Split"(%cst, %input) : (tensor<i32>, tensor<4x8xf32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testSplitSmallSplitDim(%input: tensor<4x8xf32>) {
  %cst = "tf.Const"() {value = dense<0> : tensor<i32>} : () -> tensor<i32>
  // expected-error @+1 {{dimension #0 not divisible by the number of result tensors}}
  %0:3 = "tf.Split"(%cst, %input) : (tensor<i32>, tensor<4x8xf32>) -> (tensor<*xf32>, tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testTernaryEinsum(%arg0: tensor<2x3xf32>){
  // expected-error @+1 {{supports at most two operands}}
  %0 = "tf.Einsum"(%arg0, %arg0, %arg0) {equation = "ab,cd,ef->"} : (tensor<2x3xf32>, tensor<2x3xf32>, tensor<2x3xf32>) -> (tensor<*xf32>)
  return
}

// -----

func @testTopKV2WrongInputRank(%input: tensor<f32>, %k: tensor<i32>) {
  // expected-error @+1 {{op requires input operand to have at least 1 dimension}}
  %0:2 = "tf.TopKV2"(%input, %k) : (tensor<f32>, tensor<i32>) -> (tensor<*xf32>, tensor<*xi32>)
  return
}

// -----

func @testTopKV2WrongKRank(%input: tensor<8xf32>, %k: tensor<5xi32>) {
  // expected-error @+1 {{op requires k operand to be 0D tensor}}
  %0:2 = "tf.TopKV2"(%input, %k) : (tensor<8xf32>, tensor<5xi32>) -> (tensor<*xf32>, tensor<*xi32>)
  return
}

// -----

func @testSplitVScalarInput(%input: tensor<f32>, %split_sizes: tensor<2xi32>, %split_dim: tensor<i32>) {
  // expected-error @+1 {{cannot split scalar input tensor}}
  %0:2 = "tf.SplitV"(%input, %split_sizes, %split_dim) : (tensor<f32>, tensor<2xi32>, tensor<i32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testSplitVNonScalarSplitDim(%input: tensor<4x4xf32>, %split_sizes: tensor<2xi32>, %split_dim: tensor<1xi32>) {
  // expected-error @+1 {{split dimension should be an integer scalar tensor}}
  %0:2 = "tf.SplitV"(%input, %split_sizes, %split_dim) : (tensor<4x4xf32>, tensor<2xi32>, tensor<1xi32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testSplitVSplitDimOutOfRange(%input: tensor<4x4xf32>, %split_sizes: tensor<2xi32>) {
  %split_dim = "tf.Const"() {value = dense<100>: tensor<i32>} : () -> (tensor<i32>)
  // expected-error @+1 {{split dimension must be in range [-2, 2)}}
  %0:2 = "tf.SplitV"(%input, %split_sizes, %split_dim) : (tensor<4x4xf32>, tensor<2xi32>, tensor<i32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testSplitVWrongSplitSizesType(%input: tensor<4x4xf32>, %split_sizes: tensor<2x2xi32>, %split_dim: tensor<i32>) {
  // expected-error @+1 {{op split sizes should be a 1D tensor of 2 elements}}
  %0:2 = "tf.SplitV"(%input, %split_sizes, %split_dim) : (tensor<4x4xf32>, tensor<2x2xi32>, tensor<i32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testSplitVMultipleDynamicSizes(%input: tensor<4x4xf32>) {
  %split_dim = "tf.Const"() {value = dense<1>: tensor<i32>} : () -> (tensor<i32>)
  %split_sizes = "tf.Const"() {value = dense<[-1, -1]>: tensor<2xi32>} : () -> (tensor<2xi32>)
  // expected-error @+1 {{cannot have more than one dynamic dimension in split sizes}}
  %0:2 = "tf.SplitV"(%input, %split_sizes, %split_dim) : (tensor<4x4xf32>, tensor<2xi32>, tensor<i32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testSplitVSplitSizeOutOfRange(%input: tensor<4x4xf32>) {
  %split_dim = "tf.Const"() {value = dense<1>: tensor<i32>} : () -> (tensor<i32>)
  %split_sizes = "tf.Const"() {value = dense<[-1, 100]>: tensor<2xi32>} : () -> (tensor<2xi32>)
  // expected-error @+1 {{split sizes must sum up to be less than or equal to the dimension size along split dimension, found 100 vs 4}}
  %0:2 = "tf.SplitV"(%input, %split_sizes, %split_dim) : (tensor<4x4xf32>, tensor<2xi32>, tensor<i32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testSplitVSplitSizeOutOfRange(%input: tensor<4x4xf32>) {
  %split_dim = "tf.Const"() {value = dense<1>: tensor<i32>} : () -> (tensor<i32>)
  %split_sizes = "tf.Const"() {value = dense<[2, 3]>: tensor<2xi32>} : () -> (tensor<2xi32>)
  // expected-error @+1 {{split sizes must sum up to the dimension size along split dimension, found 5 vs 4}}
  %0:2 = "tf.SplitV"(%input, %split_sizes, %split_dim) : (tensor<4x4xf32>, tensor<2xi32>, tensor<i32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testSplitV1(%input: tensor<4x4xf32>) {
  %split_dim = "tf.Const"() {value = dense<1>: tensor<i32>} : () -> (tensor<i32>)
  %split_sizes = "tf.Const"() {value = dense<[-1, 4]>: tensor<2xi32>} : () -> (tensor<2xi32>)
  %0:2 = "tf.SplitV"(%input, %split_sizes, %split_dim) : (tensor<4x4xf32>, tensor<2xi32>, tensor<i32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

func @testSplitV2(%input: tensor<4x4xf32>) {
  %split_dim = "tf.Const"() {value = dense<1>: tensor<i32>} : () -> (tensor<i32>)
  %split_sizes = "tf.Const"() {value = dense<[3, 1]>: tensor<2xi32>} : () -> (tensor<2xi32>)
  %0:2 = "tf.SplitV"(%input, %split_sizes, %split_dim) : (tensor<4x4xf32>, tensor<2xi32>, tensor<i32>) -> (tensor<*xf32>, tensor<*xf32>)
  return
}

// -----

func @testSplitVDynamic(%arg0: tensor<?x?xf32>, %arg1: tensor<?xi32>, %arg2: tensor<i32>) -> (tensor<?x?xf32>, tensor<?x?xf32>) {
  %0:2 = "tf.SplitV"(%arg0, %arg1, %arg2) : (tensor<?x?xf32>, tensor<?xi32>, tensor<i32>) -> (tensor<?x?xf32>, tensor<?x?xf32>)
  return %0#0, %0#1 : tensor<?x?xf32>, tensor<?x?xf32>
}

// -----

//===--------------------------------------------------------------------===//
//  tf.All
//===--------------------------------------------------------------------===//

func @testAllDimWrongRank(%input: tensor<4x6xi1>, %dims: tensor<2x2xi32>) {
  // expected-error @+1 {{dimensions can only be 0D or 1D tensor}}
  %0 = "tf.All"(%input, %dims) : (tensor<4x6xi1>, tensor<2x2xi32>) -> (tensor<*xi1>)
  return
}

// -----

func @testAllDimOutOfRange(%input: tensor<4x6xi1>) {
  %dims = "tf.Const"() {value = dense<[-1, 5]> : tensor<2xi32>} : () -> (tensor<2xi32>)
  // expected-error @+1 {{1-th dimension should be in the range of [-2, 2)}}
  %0 = "tf.All"(%input, %dims) : (tensor<4x6xi1>, tensor<2xi32>) -> (tensor<*xi1>)
  return
}

// -----

//===--------------------------------------------------------------------===//
//  tf.Any
//===--------------------------------------------------------------------===//

func @testAnyDimWrongRank(%input: tensor<4x6xi1>, %dims: tensor<2x2xi32>) {
  // expected-error @+1 {{dimensions can only be 0D or 1D tensor}}
  %0 = "tf.Any"(%input, %dims) : (tensor<4x6xi1>, tensor<2x2xi32>) -> (tensor<*xi1>)
  return
}

// -----

func @testAnyDimOutOfRange(%input: tensor<4x6xi1>) {
  %dims = "tf.Const"() {value = dense<[-1, 5]> : tensor<2xi32>} : () -> (tensor<2xi32>)
  // expected-error @+1 {{1-th dimension should be in the range of [-2, 2)}}
  %0 = "tf.Any"(%input, %dims) : (tensor<4x6xi1>, tensor<2xi32>) -> (tensor<*xi1>)
  return
}

// -----

//===--------------------------------------------------------------------===//
//  tf.Unpack
//===--------------------------------------------------------------------===//

func @testUnpackAxisOutOfRange(%input: tensor<2x6xf32>) {
  // expected-error @+1 {{axis attribute must be in the range of [-2, 2)}}
  %0:2 = "tf.Unpack"(%input) {axis = 5} : (tensor<2x6xf32>) -> (tensor<6xf32>, tensor<6xf32>)
  return
}

// -----

func @testAxisUnknownDim(%input: tensor<?x6xf32>) {
  // CHECK: tf.Unpack
  %0:2 = "tf.Unpack"(%input) {axis = 0} : (tensor<?x6xf32>) -> (tensor<6xf32>, tensor<6xf32>)
  return
}

// -----

func @testAxisDim(%input: tensor<2x6xf32>) {
  // expected-error @+1 {{result count must be equal to 6}}
  %0:2 = "tf.Unpack"(%input) {axis = -1} : (tensor<2x6xf32>) -> (tensor<6xf32>, tensor<6xf32>)
  return
}

// -----

//===--------------------------------------------------------------------===//
//  tf.UnsortedSegment{Max|Min|Prod|Sum}
//===--------------------------------------------------------------------===//

// CHECK-LABEL: unsortedSegmentReduction
func @unsortedSegmentReduction(%data: tensor<?x10x8xf32>, %segment_ids: tensor<7x?xi32>, %num_segments: tensor<i32>) {
  // CHECK: tf.UnsortedSegmentMin
  %0 = "tf.UnsortedSegmentMin"(%data, %segment_ids, %num_segments) : (tensor<?x10x8xf32>, tensor<7x?xi32>, tensor<i32>) -> (tensor<?x8xf32>)
  return
}

// -----

func @unsortedSegmentReduction(%data: tensor<7x10x8xf32>, %segment_ids: tensor<7x10xi32>, %num_segments: tensor<2x3xi32>) {
  // expected-error @+1 {{number of segments should be a 0-D tensor}}
  %0 = "tf.UnsortedSegmentMax"(%data, %segment_ids, %num_segments) : (tensor<7x10x8xf32>, tensor<7x10xi32>, tensor<2x3xi32>) -> (tensor<?x8xf32>)
  return
}

// -----

func @unsortedSegmentReduction(%data: tensor<7x10x8xf32>, %segment_ids: tensor<7x9xi32>, %num_segments: tensor<i32>) {
  // expected-error @+1 {{requires segment ids shape to be a prefix of data shape, but dimension #1 differs: 9 vs. 10}}
  %0 = "tf.UnsortedSegmentProd"(%data, %segment_ids, %num_segments) : (tensor<7x10x8xf32>, tensor<7x9xi32>, tensor<i32>) -> (tensor<?x8xf32>)
  return
}

// -----

func @unsortedSegmentReduction(%data: tensor<7x10x8xf32>, %segment_ids: tensor<7x10x8x1xi32>, %num_segments: tensor<i32>) {
  // expected-error @+1 {{requires segment ids rank to be less than or equal to data's rank}}
  %0 = "tf.UnsortedSegmentSum"(%data, %segment_ids, %num_segments) : (tensor<7x10x8xf32>, tensor<7x10x8x1xi32>, tensor<i32>) -> (tensor<?x8xf32>)
  return
}

// -----

func @unsortedSegmentReduction(%data: tensor<7x10x8xf32>, %segment_ids: tensor<7x10xi32>) {
  %num_segments = "tf.Const"() {value = dense<-5> : tensor<i32>} : () -> (tensor<i32>)
  // expected-error @+1 {{num of segments cannot be negative}}
  %0 = "tf.UnsortedSegmentSum"(%data, %segment_ids, %num_segments) : (tensor<7x10x8xf32>, tensor<7x10xi32>, tensor<i32>) -> (tensor<?x8xf32>)
  return
}

// -----


//===--------------------------------------------------------------------===//
//  tf.GatherV2
//===--------------------------------------------------------------------===//

func @testGatherV2(%arg0: tensor<16x2x3xf32>, %arg1: tensor<16x5xi32>) -> tensor<16x2x5x3xf32> {
  %0 = "tf.Const"() { value = dense<[-1]> : tensor<1xi32> } : () -> tensor<1xi32>
  %1 = "tf.GatherV2"(%arg0, %arg1, %0) {batch_dims = -1 : i64} : (tensor<16x2x3xf32>, tensor<16x5xi32>, tensor<1xi32>) -> tensor<16x2x5x3xf32>
  return %1 : tensor<16x2x5x3xf32>
}

// -----

// Verify that the batch_dims can be equal to the rank of the indices.
func @testGatherV2(%arg0: tensor<16x4xf32>, %arg1: tensor<16xi32>) -> tensor<16xf32> {
  %0 = "tf.Const"() { value = dense<[1]> : tensor<1xi32> } : () -> tensor<1xi32>
  %1 = "tf.GatherV2"(%arg0, %arg1, %0) {batch_dims = 1 : i64} : (tensor<16x4xf32>, tensor<16xi32>, tensor<1xi32>) -> tensor<16xf32>
  return %1 : tensor<16xf32>
}

// -----

func @testGatherV2(%arg0: tensor<16x2x3xf32>, %arg1: tensor<16x5xi32>) -> tensor<16x2x5x3xf32> {
  %0 = "tf.Const"() { value = dense<[-1]> : tensor<1xi32> } : () -> tensor<1xi32>
  // expected-error @+1 {{batch_dims (-3) must be in range [-2, 3)}}
  %1 = "tf.GatherV2"(%arg0, %arg1, %0) {batch_dims = -3 : i64} : (tensor<16x2x3xf32>, tensor<16x5xi32>, tensor<1xi32>) -> tensor<16x2x5x3xf32>
  return %1 : tensor<16x2x5x3xf32>
}

// -----

func @testGatherV2(%arg0: tensor<16x2x3xf32>, %arg1: tensor<16x5xi32>) -> tensor<16x2x5x3xf32> {
  %0 = "tf.Const"() { value = dense<[[-4]]> : tensor<1x1xi32> } : () -> tensor<1x1xi32>
  // expected-error @+1 {{requires axis to have rank at most 1}}
  %1 = "tf.GatherV2"(%arg0, %arg1, %0) {batch_dims = -1 : i64} : (tensor<16x2x3xf32>, tensor<16x5xi32>, tensor<1x1xi32>) -> tensor<16x2x5x3xf32>
  return %1 : tensor<16x2x5x3xf32>
}

// -----

func @testGatherV2(%arg0: tensor<16x2x3xf32>, %arg1: tensor<16x5xi32>) -> tensor<16x2x5x3xf32> {
  %0 = "tf.Const"() { value = dense<[-4]> : tensor<1xi32> } : () -> tensor<1xi32>
  // expected-error @+1 {{axis (-4) must be in range [-3, 3)}}
  %1 = "tf.GatherV2"(%arg0, %arg1, %0) {batch_dims = -1 : i64} : (tensor<16x2x3xf32>, tensor<16x5xi32>, tensor<1xi32>) -> tensor<16x2x5x3xf32>
  return %1 : tensor<16x2x5x3xf32>
}

// -----

func @testGatherV2(%arg0: tensor<16x2x3xf32>, %arg1: tensor<16x5xi32>) -> tensor<16x2x5x3xf32> {
  %0 = "tf.Const"() { value = dense<[0]> : tensor<1xi32> } : () -> tensor<1xi32>
  // expected-error @+1 {{requires axis (0) to be greater than or equal to batch_dims (1)}}
  %1 = "tf.GatherV2"(%arg0, %arg1, %0) {batch_dims = -1 : i64} : (tensor<16x2x3xf32>, tensor<16x5xi32>, tensor<1xi32>) -> tensor<16x2x5x3xf32>
  return %1 : tensor<16x2x5x3xf32>
}

// -----

//===--------------------------------------------------------------------===//
//  tf.StridedSliceGrad
//===--------------------------------------------------------------------===//

func @stridedSliceGrad(%dy: tensor<4x8xf32>, %begin: tensor<2xi64>, %end: tensor<2xi64>, %strides: tensor<2xi64>, %shape: tensor<2xi64>) -> tensor<?x?xf32> {
  // CHECK: tf.StridedSliceGrad
  %0 = "tf.StridedSliceGrad"(%shape, %begin, %end, %strides, %dy) : (tensor<2xi64>, tensor<2xi64>, tensor<2xi64>, tensor<2xi64>, tensor<4x8xf32>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @stridedSliceGrad(%dy: tensor<4x8xf32>, %begin: tensor<i64>, %end: tensor<2xi64>, %strides: tensor<2xi64>, %shape: tensor<2xi64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{requires begin, end and strides to be 1D tensors}}
  %0 = "tf.StridedSliceGrad"(%shape, %begin, %end, %strides, %dy) : (tensor<2xi64>, tensor<i64>, tensor<2xi64>, tensor<2xi64>, tensor<4x8xf32>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @stridedSliceGrad(%dy: tensor<4x8xf32>, %begin: tensor<32xi64>, %end: tensor<2xi64>, %strides: tensor<2xi64>, %shape: tensor<2xi64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{with less than 32 elements}}
  %0 = "tf.StridedSliceGrad"(%shape, %begin, %end, %strides, %dy) : (tensor<2xi64>, tensor<32xi64>, tensor<2xi64>, tensor<2xi64>, tensor<4x8xf32>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @stridedSliceGrad(%dy: tensor<4x8xf32>, %begin: tensor<?xi64>, %end: tensor<3xi64>, %strides: tensor<2xi64>, %shape: tensor<2xi64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{have the same number of elements}}
  %0 = "tf.StridedSliceGrad"(%shape, %begin, %end, %strides, %dy) : (tensor<2xi64>, tensor<?xi64>, tensor<3xi64>, tensor<2xi64>, tensor<4x8xf32>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @stridedSliceGrad(%dy: tensor<4x8xf32>, %shape: tensor<2xi64>) -> tensor<?x?xf32> {
  %begin = "tf.Const"() { value = dense<[0, 0]> : tensor<2xi64> } : () -> tensor<?xi64>
  %end = "tf.Const"() { value = dense<[5, 10]> : tensor<2xi64> } : () -> tensor<?xi64>
  %strides = "tf.Const"() { value = dense<[2, 3, 4]> : tensor<3xi64> } : () -> tensor<?xi64>

  // expected-error @+1 {{have the same number of elements}}
  %0 = "tf.StridedSliceGrad"(%shape, %begin, %end, %strides, %dy) : (tensor<2xi64>, tensor<?xi64>, tensor<?xi64>, tensor<?xi64>, tensor<4x8xf32>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @stridedSliceGrad(%dy: tensor<4x8xf32>, %begin: tensor<2xi64>, %end: tensor<2xi64>, %shape: tensor<2xi64>) -> tensor<?x?xf32> {
  %strides = "tf.Const"() { value = dense<[2, 0]> : tensor<2xi32> } : () -> tensor<2xi32>

  // expected-error @+1 {{requires non-zero strides}}
  %0 = "tf.StridedSliceGrad"(%shape, %begin, %end, %strides, %dy) : (tensor<2xi64>, tensor<2xi64>, tensor<2xi64>, tensor<2xi32>, tensor<4x8xf32>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @stridedSliceGrad(%dy: tensor<4x8xf32>, %begin: tensor<2xi64>, %end: tensor<2xi64>, %strides: tensor<2xi64>, %shape: tensor<2xi64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{cannot have multiple ellipses}}
  %0 = "tf.StridedSliceGrad"(%shape, %begin, %end, %strides, %dy) {ellipsis_mask = 3} : (tensor<2xi64>, tensor<2xi64>, tensor<2xi64>, tensor<2xi64>, tensor<4x8xf32>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @stridedSliceGrad(%dy: tensor<4x8xf32>, %begin: tensor<2xi64>, %end: tensor<2xi64>, %strides: tensor<2xi64>, %shape: tensor<1x2xi64>) -> tensor<?x?xf32> {
  // expected-error @+1 {{'shape' operand must be 1D tensor, but got 2D tensor}}
  %0 = "tf.StridedSliceGrad"(%shape, %begin, %end, %strides, %dy) : (tensor<1x2xi64>, tensor<2xi64>, tensor<2xi64>, tensor<2xi64>, tensor<4x8xf32>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @testDynamicStitch(%arg0: tensor<2x2xf32>) -> tensor<2x2xf32> {
  %indices = "tf.Const"() {value = dense<[1, 0]> : tensor<2xi32>} : () -> tensor<2xi32>
  %0 = "tf.DynamicStitch"(%indices, %arg0) : (tensor<2xi32>, tensor<2x2xf32>) -> tensor<2x2xf32>
  return %0 : tensor<2x2xf32>
}

// -----

func @testDynamicStitch() -> tensor<2x2xf32> {
  // expected-error @+1 {{requires attribute N with value >= 1}}
  %0 = "tf.DynamicStitch"() : () -> (tensor<2x2xf32>)
  return %0 : tensor<2x2xf32>
}

// -----

func @testDynamicStitch(%arg0: tensor<2x2xf32>) -> tensor<f32> {
  %indices = "tf.Const"() {value = dense<[1, 0]> : tensor<2xi32>} : () -> tensor<2xi32>
  // expected-error @+1 {{requires non scalar output}}
  %0 = "tf.DynamicStitch"(%indices, %arg0) : (tensor<2xi32>, tensor<2x2xf32>) -> tensor<f32>
  return %0 : tensor<f32>
}

// -----

func @testDynamicStitch(%arg0: tensor<2x2xf32>) -> tensor<2x2xf32> {
  %indices = "tf.Const"() {value = dense<[-1, 0]> : tensor<2xi32>} : () -> tensor<2xi32>
  // expected-error @+1 {{requires non-negative index values; found -1}}
  %0 = "tf.DynamicStitch"(%indices, %arg0) : (tensor<2xi32>, tensor<2x2xf32>) -> tensor<2x2xf32>
  return %0 : tensor<2x2xf32>
}

// -----

func @testDynamicStitch(%arg0: tensor<3x2xf32>) -> tensor<2x2xf32> {
  %indices = "tf.Const"() {value = dense<[1, 0]> : tensor<2xi32>} : () -> tensor<2xi32>
  // expected-error @+1 {{requires shape of data with type 'tensor<3x2xf32>' to have prefix matching with shape of the corresponding index type 'tensor<2xi32>'}}
  %0 = "tf.DynamicStitch"(%indices, %arg0) : (tensor<2xi32>, tensor<3x2xf32>) -> tensor<2x2xf32>
  return %0 : tensor<2x2xf32>
}

// -----

func @testDynamicStitch(%arg0: tensor<2xf32>, %arg1: tensor<2x2x3xf32>) -> (tensor<5x2xf32>) {
  %indices0 = "tf.Const"() {value = dense<4> : tensor<i32>} : () -> tensor<i32>
  %indices1 = "tf.Const"() {value = dense<[[3, 2], [1, 0]]> : tensor<2x2xi32>} : () -> tensor<2x2xi32>

  // expected-error @+1 {{inconsistent shaped data and index pairs; inferred item shapes [2] and [3] don't match}}
  %0 = "tf.DynamicStitch"(%indices0, %indices1, %arg0, %arg1) : (tensor<i32>, tensor<2x2xi32>, tensor<2xf32>, tensor<2x2x3xf32>) -> tensor<5x2xf32>
  return %0 : tensor<5x2xf32>
}

// -----

func @testDynamicStitch(%arg0: tensor<2x2xf32>) -> tensor<2x2xf32> {
  %indices = "tf.Const"() {value = dense<[2, 0]> : tensor<2xi32>} : () -> tensor<2xi32>
  // expected-error @+1 {{missing index 1}}
  %0 = "tf.DynamicStitch"(%indices, %arg0) : (tensor<2xi32>, tensor<2x2xf32>) -> tensor<2x2xf32>
  return %0 : tensor<2x2xf32>
}

// -----

func @testDynamicStitch(%arg0: tensor<2x2xf32>) -> tensor<3x2xf32> {
  %indices = "tf.Const"() {value = dense<[1, 0]> : tensor<2xi32>} : () -> tensor<2xi32>
  // expected-error @+1 {{has invalid output type; should be compatible with inferred type 'tensor<2x2xf32>'}}
  %0 = "tf.DynamicStitch"(%indices, %arg0) : (tensor<2xi32>, tensor<2x2xf32>) -> tensor<3x2xf32>
  return %0 : tensor<3x2xf32>
}

// -----

func @testDynamicStitch(%arg0: tensor<?x2xi32>, %arg1: tensor<?x3x3xf32>) -> (tensor<*xf32>) {
  // expected-error @+1 {{requires shape of data with type 'tensor<?x3x3xf32>' to have prefix matching with shape of the corresponding index type 'tensor<?x2xi32>'}}
  %0 = "tf.DynamicStitch"(%arg0, %arg1) : (tensor<?x2xi32>, tensor<?x3x3xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @testDynamicStitch(%arg0: tensor<?x3xf32>, %arg1: tensor<2x?xf32>) -> (tensor<2x3x2xf32>) {
  %indices0 = "tf.Const"() {value = dense<1> : tensor<i32>} : () -> tensor<i32>
  %indices1 = "tf.Const"() {value = dense<0> : tensor<i32>} : () -> tensor<i32>

  // expected-error @+1 {{has invalid output type; should be compatible with inferred type 'tensor<2x2x3xf32>'}}
  %0 = "tf.DynamicStitch"(%indices0, %indices1, %arg0, %arg1) : (tensor<i32>, tensor<i32>, tensor<?x3xf32>, tensor<2x?xf32>) -> tensor<2x3x2xf32>
  return %0 : tensor<2x3x2xf32>
}

// -----

func @testConcatOffest(%concat_dim: tensor<i32>, %shape0: tensor<3xi32>) {
  // expected-error @+1 {{'tf.ConcatOffset' op requires N to be at least 2, got 1}}
  %0 = "tf.ConcatOffset"(%concat_dim, %shape0) : (tensor<i32>, tensor<3xi32>) -> tensor<3xi32>
  return
}

// -----

func @testConcatOffest(%concat_dim: tensor<i32>, %shape0: tensor<3xi32>, %shape1: tensor<3xi32>) {
  // expected-error @+1 {{'tf.ConcatOffset' op requires sizes of shapes and offsets to be the same, got sizes 2 and 3}}
  %0:3 = "tf.ConcatOffset"(%concat_dim, %shape0, %shape1) : (tensor<i32>, tensor<3xi32>, tensor<3xi32>) -> (tensor<3xi32>, tensor<3xi32>, tensor<3xi32>)
  return
}

// -----

func @testConcatOffest(%concat_dim: tensor<1xi32>, %shape0: tensor<3xi32>, %shape1: tensor<3xi32>) {
  // expected-error @+1 {{'tf.ConcatOffset' op requires concat_dim to be a scalar, got tensor of rank 1}}
  %0:2 = "tf.ConcatOffset"(%concat_dim, %shape0, %shape1) : (tensor<1xi32>, tensor<3xi32>, tensor<3xi32>) -> (tensor<3xi32>, tensor<3xi32>)
  return
}

// -----

func @testConcatOffest(%concat_dim: tensor<i32>, %shape0: tensor<3xi32>, %shape1: tensor<3xi32>) {
  // expected-error @+1 {{'tf.ConcatOffset' op requires operand and result 1 to have compatible shapes}}
  %0:2 = "tf.ConcatOffset"(%concat_dim, %shape0, %shape1) : (tensor<i32>, tensor<3xi32>, tensor<3xi32>) -> (tensor<3xi32>, tensor<8xi32>)
  return
}

// -----

func @testConcatOffest(%concat_dim: tensor<i32>, %shape0: tensor<3xi32>, %shape1: tensor<3x3xi32>) {
  // expected-error @+1 {{'tf.ConcatOffset' op requires shape tensor operand 1 to be of rank 1, got tensor of rank 2}}
  %0:2 = "tf.ConcatOffset"(%concat_dim, %shape0, %shape1) : (tensor<i32>, tensor<3xi32>, tensor<3x3xi32>) -> (tensor<3xi32>, tensor<3x3xi32>)
  return
}

// -----

func @testConcatOffest(%concat_dim: tensor<i32>, %shape0: tensor<3xi32>, %shape1: tensor<8xi32>) {
  // expected-error @+1 {{'tf.ConcatOffset' op requires shape tensor (rank 1) operand 1 to be of length 3, got tensor (rank 1) of length 8}}
  %0:2 = "tf.ConcatOffset"(%concat_dim, %shape0, %shape1) : (tensor<i32>, tensor<3xi32>, tensor<8xi32>) -> (tensor<3xi32>, tensor<8xi32>)
  return
}

// -----

func @tensor_scatter_update(%tensor: tensor<f32>, %indices: tensor<4x2xi32>, %updates: tensor<4x4xf32>) -> tensor<f32> {
  // expected-error @+1 {{op requires tensor operand to have at least 1 dimension}}
  %0 = "tf.TensorScatterUpdate"(%tensor, %indices, %updates) : (tensor<f32>, tensor<4x2xi32>, tensor<4x4xf32>) -> tensor<f32>
  return %0 : tensor<f32>
}

// -----

func @tensor_scatter_update(%tensor: tensor<4x4x4xf32>, %indices: tensor<i32>, %updates: tensor<4x4xf32>) -> tensor<4x4x4xf32> {
  // expected-error @+1 {{op requires indices operand to have at least 1 dimension}}
  %0 = "tf.TensorScatterUpdate"(%tensor, %indices, %updates) : (tensor<4x4x4xf32>, tensor<i32>, tensor<4x4xf32>) -> tensor<4x4x4xf32>
  return %0 : tensor<4x4x4xf32>
}

// -----

func @tensor_scatter_update(%tensor: tensor<4x4x4xf32>, %indices: tensor<4x2xi32>, %updates: tensor<f32>) -> tensor<4x4x4xf32> {
  // expected-error @+1 {{op requires updates operand to have at least 1 dimension}}
  %0 = "tf.TensorScatterUpdate"(%tensor, %indices, %updates) : (tensor<4x4x4xf32>, tensor<4x2xi32>, tensor<f32>) -> tensor<4x4x4xf32>
  return %0 : tensor<4x4x4xf32>
}

// -----

func @tensor_scatter_update(%tensor: tensor<4xf32>, %indices: tensor<4x2xi32>, %updates: tensor<4x4xf32>) -> tensor<4x4x4xf32> {
  // expected-error @+1 {{op requires tensor operand with rank greater than or equal to the indices operand's last dimensions}}
  %0 = "tf.TensorScatterUpdate"(%tensor, %indices, %updates) : (tensor<4xf32>, tensor<4x2xi32>, tensor<4x4xf32>) -> tensor<4x4x4xf32>
  return %0 : tensor<4x4x4xf32>
}

// -----

// CHECK-LABEL: func @testParseExampleV2DenseOnlyValid
func @testParseExampleV2DenseOnlyValid(%serialized: tensor<32x!tf.string>, %names : tensor<32x!tf.string>, %dense_keys : tensor<2x!tf.string>, %dense_default_0 : tensor<?xf32>, %dense_default_1 : tensor<?xf32>) -> (tensor<32xf32>) {
  %empty_str_vector = "tf.Const"() {dtype = !tf.string, value = opaque<"tf", "0x746674656E736F722464747970653A2044545F535452494E472074656E736F725F7368617065207B2064696D207B207D207D"> : tensor<0x!tf.string>} : () -> tensor<0x!tf.string>
  %result:2 = "tf.ParseExampleV2"(%serialized, %names, %empty_str_vector, %dense_keys, %empty_str_vector, %dense_default_0, %dense_default_1) {dense_shapes = [#tf.shape<>, #tf.shape<>], num_sparse = 0 : i64, result_segment_sizes = dense<[0, 0, 0, 2, 0, 0]> : vector<6xi32>} : (tensor<32x!tf.string>, tensor<32x!tf.string>, tensor<0x!tf.string>, tensor<2x!tf.string>, tensor<0x!tf.string>, tensor<?xf32>, tensor<?xf32>) -> (tensor<32xf32>, tensor<32xf32>)
  return %result#0 : tensor<32xf32>
}

// -----

func @testParseExampleV2DenseMismatchedInputOutput(%serialized: tensor<32x!tf.string>, %names : tensor<32x!tf.string>, %dense_keys : tensor<2x!tf.string>, %dense_default_0 : tensor<?xf32>, %dense_default_1 : tensor<?xf32>) -> (tensor<32xf32>) {
  %empty_str_vector = "tf.Const"() {dtype = !tf.string, value = opaque<"tf", "0x746674656E736F722464747970653A2044545F535452494E472074656E736F725F7368617065207B2064696D207B207D207D"> : tensor<0x!tf.string>} : () -> tensor<0x!tf.string>
  // expected-error @+1 {{output 'dense_values' should have same length as attribute 'Tdense'}}
  %result:3 = "tf.ParseExampleV2"(%serialized, %names, %empty_str_vector, %dense_keys, %empty_str_vector, %dense_default_0, %dense_default_1) {dense_shapes = [#tf.shape<>, #tf.shape<>], num_sparse = 0 : i64, result_segment_sizes = dense<[0, 0, 0, 3, 0, 0]> : vector<6xi32>} : (tensor<32x!tf.string>, tensor<32x!tf.string>, tensor<0x!tf.string>, tensor<2x!tf.string>, tensor<0x!tf.string>, tensor<?xf32>, tensor<?xf32>) -> (tensor<32xf32>, tensor<32xf32>, tensor<32xi64>)
  return %result#0 : tensor<32xf32>
}

// -----

// CHECK-LABEL: func @testParseExampleV2SparseOnlyValid
func @testParseExampleV2SparseOnlyValid(%serialized: tensor<32x!tf.string>, %names : tensor<32x!tf.string>, %sparse_keys : tensor<2x!tf.string>) -> (tensor<?x2xi64>) {
  %empty_str_vector = "tf.Const"() {dtype = !tf.string, value = opaque<"tf", "0x746674656E736F722464747970653A2044545F535452494E472074656E736F725F7368617065207B2064696D207B207D207D"> : tensor<0x!tf.string>} : () -> tensor<0x!tf.string>
  %result:6 = "tf.ParseExampleV2"(%serialized, %names, %sparse_keys, %empty_str_vector, %empty_str_vector) {dense_shapes = [], num_sparse = 2 : i64, result_segment_sizes = dense<[2, 2, 2, 0, 0, 0]> : vector<6xi32>} : (tensor<32x!tf.string>, tensor<32x!tf.string>, tensor<2x!tf.string>, tensor<0x!tf.string>, tensor<0x!tf.string>) -> (tensor<?x2xi64>, tensor<?x2xi64>, tensor<?x!tf.string>, tensor<?xi64>, tensor<2xi64>, tensor<2xi64>)
  return %result#0 : tensor<?x2xi64>
}

// -----

func @testParseExampleV2SparseInvalidNumSparse(%serialized: tensor<32x!tf.string>, %names : tensor<32x!tf.string>, %sparse_keys : tensor<2x!tf.string>) -> (tensor<?x2xi64>) {
  %empty_str_vector = "tf.Const"() {dtype = !tf.string, value = opaque<"tf", "0x746674656E736F722464747970653A2044545F535452494E472074656E736F725F7368617065207B2064696D207B207D207D"> : tensor<0x!tf.string>} : () -> tensor<0x!tf.string>
  // expected-error @+1 {{attribute 'num_sparse' should be the same as the length of attribute 'sparse_types'}}
  %result:6 = "tf.ParseExampleV2"(%serialized, %names, %sparse_keys, %empty_str_vector, %empty_str_vector) {dense_shapes = [], num_sparse = 3 : i64, result_segment_sizes = dense<[2, 2, 2, 0, 0, 0]> : vector<6xi32>} : (tensor<32x!tf.string>, tensor<32x!tf.string>, tensor<2x!tf.string>, tensor<0x!tf.string>, tensor<0x!tf.string>) -> (tensor<?x2xi64>, tensor<?x2xi64>, tensor<?x!tf.string>, tensor<?xi64>, tensor<2xi64>, tensor<2xi64>)
  return %result#0 : tensor<?x2xi64>
}

// -----

func @testParseExampleV2SparseInvalidSparseIndicesOutput(%serialized: tensor<32x!tf.string>, %names : tensor<32x!tf.string>, %sparse_keys : tensor<2x!tf.string>) -> (tensor<?x2xi64>) {
  %empty_str_vector = "tf.Const"() {dtype = !tf.string, value = opaque<"tf", "0x746674656E736F722464747970653A2044545F535452494E472074656E736F725F7368617065207B2064696D207B207D207D"> : tensor<0x!tf.string>} : () -> tensor<0x!tf.string>
  // expected-error @+1 {{output 'sparse_indices' should have same length as attribute 'sparse_types'}}
  %result:5 = "tf.ParseExampleV2"(%serialized, %names, %sparse_keys, %empty_str_vector, %empty_str_vector) {dense_shapes = [], num_sparse = 2 : i64, result_segment_sizes = dense<[1, 2, 2, 0, 0, 0]> : vector<6xi32>} : (tensor<32x!tf.string>, tensor<32x!tf.string>, tensor<2x!tf.string>, tensor<0x!tf.string>, tensor<0x!tf.string>) -> (tensor<?x2xi64>, tensor<?x!tf.string>, tensor<?xi64>, tensor<2xi64>, tensor<2xi64>)
  return %result#0 : tensor<?x2xi64>
}

// -----

func @testParseExampleV2SparseOnlyValid(%serialized: tensor<32x!tf.string>, %names : tensor<32x!tf.string>, %sparse_keys : tensor<2x!tf.string>) -> (tensor<?x2xi64>) {
  %empty_str_vector = "tf.Const"() {dtype = !tf.string, value = opaque<"tf", "0x746674656E736F722464747970653A2044545F535452494E472074656E736F725F7368617065207B2064696D207B207D207D"> : tensor<0x!tf.string>} : () -> tensor<0x!tf.string>
  // expected-error @+1 {{output 'sparse_shapes' should have same length as attribute 'sparse_types'}}
  %result:5 = "tf.ParseExampleV2"(%serialized, %names, %sparse_keys, %empty_str_vector, %empty_str_vector) {dense_shapes = [], num_sparse = 2 : i64, result_segment_sizes = dense<[2, 2, 1, 0, 0, 0]> : vector<6xi32>} : (tensor<32x!tf.string>, tensor<32x!tf.string>, tensor<2x!tf.string>, tensor<0x!tf.string>, tensor<0x!tf.string>) -> (tensor<?x2xi64>, tensor<?x2xi64>, tensor<?x!tf.string>, tensor<?xi64>, tensor<2xi64>)
  return %result#0 : tensor<?x2xi64>
}

// -----

// CHECK-LABEL: func @testParseExampleV2RaggedOnlyValid
func @testParseExampleV2RaggedOnlyValid(%serialized: tensor<32x!tf.string>, %names : tensor<32x!tf.string>, %ragged_keys : tensor<2x!tf.string>) -> (tensor<?xf32>) {
  %empty_str_vector = "tf.Const"() {dtype = !tf.string, value = opaque<"tf", "0x746674656E736F722464747970653A2044545F535452494E472074656E736F725F7368617065207B2064696D207B207D207D"> : tensor<0x!tf.string>} : () -> tensor<0x!tf.string>
  %result:4 = "tf.ParseExampleV2"(%serialized, %names, %empty_str_vector, %empty_str_vector, %ragged_keys) {dense_shapes = [], num_sparse = 0 : i64, result_segment_sizes = dense<[0, 0, 0, 0, 2, 2]> : vector<6xi32>} : (tensor<32x!tf.string>, tensor<32x!tf.string>, tensor<0x!tf.string>, tensor<0x!tf.string>, tensor<2x!tf.string>) -> (tensor<?xf32>, tensor<?x!tf.string>, tensor<?xi32>, tensor<?xi64>)
  return %result#0 : tensor<?xf32>
}

// -----

func @testParseExampleV2RaggedMismatchedOutputLengths(%serialized: tensor<32x!tf.string>, %names : tensor<32x!tf.string>, %ragged_keys : tensor<2x!tf.string>) -> (tensor<?xf32>) {
  %empty_str_vector = "tf.Const"() {dtype = !tf.string, value = opaque<"tf", "0x746674656E736F722464747970653A2044545F535452494E472074656E736F725F7368617065207B2064696D207B207D207D"> : tensor<0x!tf.string>} : () -> tensor<0x!tf.string>
  // expected-error @+1 {{attribute 'ragged_value_types' should have same length as attribute 'ragged_split_types'}}
  %result:3 = "tf.ParseExampleV2"(%serialized, %names, %empty_str_vector, %empty_str_vector, %ragged_keys) {dense_shapes = [], num_sparse = 0 : i64, result_segment_sizes = dense<[0, 0, 0, 0, 2, 1]> : vector<6xi32>} : (tensor<32x!tf.string>, tensor<32x!tf.string>, tensor<0x!tf.string>, tensor<0x!tf.string>, tensor<2x!tf.string>) -> (tensor<?xf32>, tensor<?x!tf.string>, tensor<?xi32>)
  return %result#0 : tensor<?xf32>
}

// -----

// Legal BatchMatMul op.
func @testBatchMatMul(%lhs: tensor<2x?x2x?x3x5xf32>, %rhs: tensor<2x2x?x?x5x7xf32>) {
  %0 = "tf.BatchMatMul"(%lhs, %rhs) : (tensor<2x?x2x?x3x5xf32>, tensor<2x2x?x?x5x7xf32>) -> tensor<2x?x?x?x3x7xf32>
  return
}

// -----

// Mismatching batch dimensions.
func @testBatchMatMul(%lhs: tensor<1x3x5xf32>, %rhs: tensor<2x5x7xf32>) {
  // expected-error @+1 {{found mismatching batch dimensions for lhs shape 'tensor<1x3x5xf32>' and rhs shape 'tensor<2x5x7xf32>'}}
  %0 = "tf.BatchMatMul"(%lhs, %rhs) : (tensor<1x3x5xf32>, tensor<2x5x7xf32>) -> tensor<2x3x7xf32>
}

// -----

func @testBatchMatMulV2(%lhs: tensor<f32>, %rhs: tensor<10x10xf32>) {
  // expected-error @+1 {{requires lhs operand to have rank at least two}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<f32>, tensor<10x10xf32>) -> tensor<10x10xf32>
}

// -----

func @testBatchMatMulV2(%lhs: tensor<10x10xf32>, %rhs: tensor<f32>) {
  // expected-error @+1 {{requires rhs operand to have rank at least two}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<10x10xf32>, tensor<f32>) -> tensor<10x10xf32>
}

// -----

// CHECK-LABEL: func @testBatchMatMulV2NoBatchDimension
func @testBatchMatMulV2NoBatchDimension(%lhs: tensor<5x10xf32>, %rhs: tensor<10x10xf32>) -> (tensor<5x10xf32>) {
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<5x10xf32>, tensor<10x10xf32>) -> tensor<5x10xf32>
  return %0 : tensor<5x10xf32>
}

// -----

// CHECK-LABEL: func @testBatchMatMulV2ValidBroadcastingBatchDimension
func @testBatchMatMulV2ValidBroadcastingBatchDimension(%lhs: tensor<10x2x5x10xf32>, %rhs: tensor<10x10xf32>) -> (tensor<10x2x5x10xf32>) {
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<10x2x5x10xf32>, tensor<10x10xf32>) -> tensor<10x2x5x10xf32>
  return %0 : tensor<10x2x5x10xf32>
}

// -----

// CHECK-LABEL: func @testBatchMatMulV2ValidMultiBatchDimension
func @testBatchMatMulV2ValidMultiBatchDimension(%lhs: tensor<4x5x1x3x2xf32>, %rhs: tensor<1x1x3x5xf32>) -> (tensor<4x5x1x2x5xf32>) {
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) { adj_x = true } : (tensor<4x5x1x3x2xf32>, tensor<1x1x3x5xf32>) -> tensor<4x5x1x2x5xf32>
  return %0 : tensor<4x5x1x2x5xf32>
}

// -----

func @testBatchMatMulV2InvalidBroadcastingBatchDimensionWithHigherXRank(%lhs: tensor<10x2x5x10xf32>, %rhs: tensor<10x10x10xf32>) {
  // expected-error @+1 {{found incompatible broadcast batch dimensions for lhs shape 'tensor<10x2x5x10xf32>' and rhs shape 'tensor<10x10x10xf32>'}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<10x2x5x10xf32>, tensor<10x10x10xf32>) -> tensor<10x10xf32>
}

// -----

func @testBatchMatMulV2InvalidBroadcastingBatchDimensionWithSameRank(%lhs: tensor<10x2x5x10xf32>, %rhs: tensor<10x10x10x10xf32>) {
  // expected-error @+1 {{found incompatible broadcast batch dimensions for lhs shape 'tensor<10x2x5x10xf32>' and rhs shape 'tensor<10x10x10x10xf32>'}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<10x2x5x10xf32>, tensor<10x10x10x10xf32>) -> tensor<10x10xf32>
}

// -----

func @testBatchMatMulV2InvalidBroadcastingBatchDimensionWithHigherYRank(%lhs: tensor<2x5x10xf32>, %rhs: tensor<10x10x10x10xf32>) {
  // expected-error @+1 {{found incompatible broadcast batch dimensions for lhs shape 'tensor<2x5x10xf32>' and rhs shape 'tensor<10x10x10x10xf32>'}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<2x5x10xf32>, tensor<10x10x10x10xf32>) -> tensor<10x10xf32>
}

// -----

func @testBatchMatMulV2InvalidOutputBatchDimension(%lhs: tensor<10x2x5x10xf32>, %rhs: tensor<2x10x10xf32>) {
  // expected-error @+1 {{has mismatching input batch dimension 2 and output batch dimension 3}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<10x2x5x10xf32>, tensor<2x10x10xf32>) -> tensor<10x3x10x10xf32>
}

// -----

func @testBatchMatMulV2InvalidOutputRank(%lhs: tensor<10x2x5x10xf32>, %rhs: tensor<10x1x10x10xf32>) {
  // expected-error @+1 {{found invalid output rank, expected 4 but got 3}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<10x2x5x10xf32>, tensor<10x1x10x10xf32>) -> tensor<10x5x10xf32>
}

// -----

func @testBatchMatMulV2InvalidOutputRowDim(%lhs: tensor<10x2x5x10xf32>, %rhs: tensor<10x10xf32>) {
  // expected-error @+1 {{found invalid output dimension on row, expected 5 but got 10}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<10x2x5x10xf32>, tensor<10x10xf32>) -> tensor<10x2x10x10xf32>
}

// -----

func @testBatchMatMulV2AdjXInvalidOutputRowDim(%lhs: tensor<10x2x10x5xf32>, %rhs: tensor<10x10xf32>) {
  // expected-error @+1 {{found invalid output dimension on row, expected 5 but got 10}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) { adj_x = true } : (tensor<10x2x10x5xf32>, tensor<10x10xf32>) -> tensor<10x2x10x10xf32>
}

// -----

func @testBatchMatMulV2InvalidOutputColDim(%lhs: tensor<10x2x5x10xf32>, %rhs: tensor<10x10xf32>) {
  // expected-error @+1 {{found invalid output dimension on col, expected 10 but got 5}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<10x2x5x10xf32>, tensor<10x10xf32>) -> tensor<10x2x5x5xf32>
}

// -----

func @testBatchMatMulV2AdjYInvalidOutputColDim(%lhs: tensor<10x2x5x10xf32>, %rhs: tensor<4x10xf32>) {
  // expected-error @+1 {{found invalid output dimension on col, expected 4 but got 10}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) { adj_y = true } : (tensor<10x2x5x10xf32>, tensor<4x10xf32>) -> tensor<10x2x5x10xf32>
}

// -----

// CHECK-LABEL: func @testBatchMatMulV2PartiallyKnownInputBatchDim
func @testBatchMatMulV2PartiallyKnownInputBatchDim(%lhs: tensor<4x5x?x3x2xf32>, %rhs: tensor<1x1x3x5xf32>) -> (tensor<4x5x?x2x5xf32>) {
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) { adj_x = true } : (tensor<4x5x?x3x2xf32>, tensor<1x1x3x5xf32>) -> tensor<4x5x?x2x5xf32>
  return %0 : tensor<4x5x?x2x5xf32>
}

// -----

// CHECK-LABEL: func @testBatchMatMulV2PartiallyKnownMatmulDim
func @testBatchMatMulV2PartiallyKnownMatmulDim(%lhs: tensor<4x5x1x?x3xf32>, %rhs: tensor<1x1x3x5xf32>) -> (tensor<4x5x1x?x5xf32>) {
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<4x5x1x?x3xf32>, tensor<1x1x3x5xf32>) -> tensor<4x5x1x?x5xf32>
  return %0 : tensor<4x5x1x?x5xf32>
}

// -----

func @testBatchMatMulV2InvalidPartiallyKnownMatmulDim(%lhs: tensor<4x5x1x?x3xf32>, %rhs: tensor<1x1x3x5xf32>) -> (tensor<4x5x1x?x3xf32>) {
  // expected-error @+1 {{found invalid output dimension on col, expected 5 but got 3}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) : (tensor<4x5x1x?x3xf32>, tensor<1x1x3x5xf32>) -> tensor<4x5x1x?x3xf32>
  return %0 : tensor<4x5x1x?x3xf32>
}

// -----

func @testBatchMatMulV2AdjXInvalidPartiallyKnownMatmulDim(%lhs: tensor<4x5x1x3x?xf32>, %rhs: tensor<1x1x3x5xf32>) -> (tensor<4x5x1x?x3xf32>) {
  // expected-error @+1 {{found invalid output dimension on col, expected 5 but got 3}}
  %0 = "tf.BatchMatMulV2"(%lhs, %rhs) { adj_x = true } : (tensor<4x5x1x3x?xf32>, tensor<1x1x3x5xf32>) -> tensor<4x5x1x?x3xf32>
  return %0 : tensor<4x5x1x?x3xf32>
}

// -----

func @testDataFormatVecPermuteInvalid1dInput(%x: tensor<5xi32>) {
  // expected-error @+1 {{requires 1D input of size 4}}
  %0 = "tf.DataFormatVecPermute"(%x): (tensor<5xi32>) -> tensor<5xi32>
  return
}

// -----

func @testDataFormatVecPermuteInvalid2dDim0Input(%x: tensor<5x2xi32>) {
  // expected-error @+1 {{requires first dimensions of 2D input to be of size 4}}
  %0 = "tf.DataFormatVecPermute"(%x): (tensor<5x2xi32>) -> tensor<5x2xi32>
  return
}

// -----

func @testDataFormatVecPermuteInvalid2dDim1Input(%x: tensor<4x3xi32>) {
  // expected-error @+1 {{requires second dimensions of 2D input to be of size 2}}
  %0 = "tf.DataFormatVecPermute"(%x): (tensor<4x3xi32>) -> tensor<4x3xi32>
  return
}

// -----

func @testDataFormatVecPermuteInvalid3dInput(%x: tensor<4x2x2xi32>) {
  // expected-error @+1 {{requires input of rank 1 or 2}}
  %0 = "tf.DataFormatVecPermute"(%x): (tensor<4x2x2xi32>) -> tensor<4x2x2xi32>
  return
}

// -----

func @testSendTPUEmbeddingGradients(%x: tensor<512x256xf32>) {
  "tf.SendTPUEmbeddingGradients"(%x) {N = 1 : i64, NN = 0 : i64, config = "", operand_segment_sizes = dense<[1, 0]> : vector<2xi32>} : (tensor<512x256xf32>) -> ()
  return
}

// -----

//===--------------------------------------------------------------------===//
//  tf.BatchToSpace
//===--------------------------------------------------------------------===//

func @testBatchToSpaceDynamic(%arg0: tensor<*xf32>, %arg1: tensor<*xi32>) {
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<*xf32>, tensor<*xi32>) -> tensor<*xf32>
  return
}

func @testBatchToSpaceRankedInput(%arg0: tensor<?x?x?x?xf32>, %arg1: tensor<*xi32>) {
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<?x?x?x?xf32>, tensor<*xi32>) -> tensor<*xf32>
  return
}

func @testBatchToSpaceRankedCrops(%arg0: tensor<*xf32>, %arg1: tensor<?x?xi32>) {
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<*xf32>, tensor<?x?xi32>) -> tensor<*xf32>
  return
}

func @testBatchToSpaceRankedOutput(%arg0: tensor<*xf32>, %arg1: tensor<*xi32>) {
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<*xf32>, tensor<*xi32>) -> tensor<?x?x?x?xf32>
  return
}

func @testBatchToSpaceStatic(%arg0: tensor<36x8x8x8xf32>) {
  %crops = "tf.Const"() {value = dense<[[1, 2], [3, 4]]> : tensor<2x2xi32>} : () -> tensor<2x2xi32>
  %0 = "tf.BatchToSpace"(%arg0, %crops) {block_size = 3 : i64} : (tensor<36x8x8x8xf32>, tensor<2x2xi32>) -> tensor<4x21x17x8xf32>
  return
}

// -----

func @testBatchToSpaceInvalidInputRank(%arg0: tensor<8xf32>, %arg1: tensor<*xi32>) {
  // expected-error @+1 {{'tf.BatchToSpace' op requires input to be a 4D tensor, but got 'tensor<8xf32>'}}
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<8xf32>, tensor<*xi32>) -> tensor<*xf32>
  return
}

// -----

func @testBatchToSpaceInvalidInputBatch(%arg0: tensor<2x4x6x8xf32>, %arg1: tensor<*xi32>) {
  // expected-error @+1 {{'tf.BatchToSpace' op requires input batch (dimension 0) to be evenly divisible by (block_size * block_size), but got input batch 2 and block_size 2}}
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<2x4x6x8xf32>, tensor<*xi32>) -> tensor<*xf32>
  return
}

// -----

func @testBatchToSpaceInvalidCropsRank(%arg0: tensor<*xf32>, %arg1: tensor<?x?x?xi32>) {
  // expected-error @+1 {{'tf.BatchToSpace' op requires crops to be a 2D tensor, but got 'tensor<?x?x?xi32>'}}
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<*xf32>, tensor<?x?x?xi32>) -> tensor<*xf32>
  return
}

// -----

func @testBatchToSpaceInvalidCropsFirstDim(%arg0: tensor<*xf32>, %arg1: tensor<3x?xi32>) {
  // expected-error @+1 {{'tf.BatchToSpace' op requires crops to be a tensor<2x2>, but got 'tensor<3x?xi32>'}}
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<*xf32>, tensor<3x?xi32>) -> tensor<*xf32>
  return
}

// -----

func @testBatchToSpaceInvalidCropsSecondDim(%arg0: tensor<*xf32>, %arg1: tensor<?x3xi32>) {
  // expected-error @+1 {{'tf.BatchToSpace' op requires crops to be a tensor<2x2>, but got 'tensor<?x3xi32>'}}
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<*xf32>, tensor<?x3xi32>) -> tensor<*xf32>
  return
}

// -----

func @testBatchToSpaceBadCropValues(%arg0: tensor<*xf32>) {
  %crops = "tf.Const"() {value = dense<[[-1, -2], [-3, -4]]> : tensor<2x2xi32>} : () -> tensor<2x2xi32>
  // expected-error @+1 {{'tf.BatchToSpace' op requires all crop values to be nonnegative, but got dense<[[-1, -2], [-3, -4]]> : tensor<2x2xi32>}}
  %0 = "tf.BatchToSpace"(%arg0, %crops) {block_size = 2 : i64} : (tensor<*xf32>, tensor<2x2xi32>) -> tensor<*xf32>
  return
}

// -----

func @testBatchToSpaceInvalidOutputRank(%arg0: tensor<*xf32>, %arg1: tensor<*xi32>) {
  // expected-error @+1 {{'tf.BatchToSpace' op requires output to be a 4D tensor, but got 'tensor<8xf32>'}}
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<*xf32>, tensor<*xi32>) -> tensor<8xf32>
  return
}

// -----

func @testBatchToSpaceInvalidOutputBatch(%arg0: tensor<16x8x8x3xf32>, %arg1: tensor<*xi32>) {
  // expected-error @+1 {{'tf.BatchToSpace' op requires output batch (dimension 0) to be equal to input batch (dimension 0) / (block_size * block_size), but got output batch 8, input batch 16, and block_size 2}}
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<16x8x8x3xf32>, tensor<*xi32>) -> tensor<8x8x8x3xf32>
  return
}

// -----

func @testBatchToSpaceInvalidOutputHeight(%arg0: tensor<16x8x8x3xf32>, %arg1: tensor<*xi32>) {
  // expected-error @+1 {{'tf.BatchToSpace' op requires output height (dimension 1) to be less than or equal to input height (dimension 1) * block_size, but got output height 17, input height 8, and block_size 2}}
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<16x8x8x3xf32>, tensor<*xi32>) -> tensor<4x17x8x3xf32>
  return
}

// -----

func @testBatchToSpaceInvalidOutputHeightCrops(%arg0: tensor<16x8x8x3xf32>) {
  %crops = "tf.Const"() {value = dense<[[1, 2], [3, 4]]> : tensor<2x2xi32>} : () -> tensor<2x2xi32>
  // expected-error @+1 {{'tf.BatchToSpace' op requires output height (dimension 1) to be equal to input height (dimension 1) * block_size - crop_top - crop_bottom, but got output height 8, input height 8, crop_top 1, crop_bottom 2, and block_size 2}}
  %0 = "tf.BatchToSpace"(%arg0, %crops) {block_size = 2 : i64} : (tensor<16x8x8x3xf32>, tensor<2x2xi32>) -> tensor<4x8x9x3xf32>
  return
}

// -----

func @testBatchToSpaceInvalidOutputWidth(%arg0: tensor<16x4x4x3xf32>, %arg1: tensor<*xi32>) {
  // expected-error @+1 {{'tf.BatchToSpace' op requires output width (dimension 2) to be less than or equal to input width (dimension 2) * block_size, but got output width 9, input width 4, and block_size 2}}
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<16x4x4x3xf32>, tensor<*xi32>) -> tensor<4x4x9x3xf32>
  return
}

// -----

func @testBatchToSpaceInvalidOutputWidthCrops(%arg0: tensor<16x8x8x3xf32>) {
  %crops = "tf.Const"() {value = dense<[[1, 2], [3, 4]]> : tensor<2x2xi32>} : () -> tensor<2x2xi32>
  // expected-error @+1 {{'tf.BatchToSpace' op requires output width (dimension 2) to be equal to input width (dimension 2) * block_size - crop_left - crop_right, but got output width 8, input width 8, crop_left 3, crop_right 4, and block_size 2}}
  %0 = "tf.BatchToSpace"(%arg0, %crops) {block_size = 2 : i64} : (tensor<16x8x8x3xf32>, tensor<2x2xi32>) -> tensor<4x13x8x3xf32>
  return
}

// -----

func @testBatchToSpaceInvalidOutputDepth(%arg0: tensor<16x8x8x3xf32>, %arg1: tensor<*xi32>) {
  // expected-error @+1 {{'tf.BatchToSpace' op requires output depth (dimension 3) to be equal to input depth (dimension 3), but got output depth 8 and input depth 3}}
  %0 = "tf.BatchToSpace"(%arg0, %arg1) {block_size = 2 : i64} : (tensor<16x8x8x3xf32>, tensor<*xi32>) -> tensor<4x8x8x8xf32>
  return
}

// -----

func private @branch()

func @testCaseBadBranchIndicesShape(%arg0: tensor<8xi32>) {
  // expected-error @+1 {{expects 'branch_index' to be a scalar, but got 'tensor<8xi32>'}}
  "tf.Case"(%arg0) {branches = [@branch], is_stateless = false} : (tensor<8xi32>) -> ()
  return
}

// -----

func private @branch0(tensor<2xf32>, tensor<2xf32>) -> tensor<2xf32>
func private @branch1(tensor<2xf32>) -> tensor<2xf32>

func @testCaseMismatchedNumOperands(%arg0: tensor<i32>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{'tf.Case' op branch #0 inputs (size = 2) should have the same number of values as inputs (size = 1)}}
  %0 = "tf.Case"(%arg0, %arg1) {branches = [@branch0, @branch1], is_stateless = false} : (tensor<i32>, tensor<2xf32>) -> tensor<2xf32>
  return %0 : tensor<2xf32>
}

// -----

func private @branch0(tensor<2xf32>) -> (tensor<2xf32>, tensor<2xf32>)
func private @branch1(tensor<2xf32>) -> tensor<2xf32>

func @testCaseMismatchedNumResults(%arg0: tensor<i32>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{'tf.Case' op branch #0 results (size = 2) should have the same number of values as results (size = 1)}}
  %0 = "tf.Case"(%arg0, %arg1) {branches = [@branch0, @branch1], is_stateless = false} : (tensor<i32>, tensor<2xf32>) -> tensor<2xf32>
  return %0 : tensor<2xf32>
}

// -----

func private @branch0(tensor<*xf16>) -> tensor<*xf32>
func private @branch1(tensor<*xf32>) -> tensor<*xf32>

func @testCaseOperandNotCastCompatible(%arg0: tensor<i32>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{'tf.Case' op branch #0 input type tensor<*xf16> is incompatible with input type tensor<2xf32> at index 0}}
  %0 = "tf.Case"(%arg0, %arg1) {branches = [@branch0, @branch1], is_stateless = false} : (tensor<i32>, tensor<2xf32>) -> tensor<2xf32>
  return %0 : tensor<2xf32>
}

// -----

func private @branch0(tensor<2xf32>) -> tensor<*xf32>
func private @branch1(tensor<3xf32>) -> tensor<*xf32>

func @testCaseBranchArgumentsNotCastCompatible(%arg0: tensor<i32>, %arg1: tensor<*xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{expects all branch input type(s) (tensor<2xf32>, tensor<3xf32>) at index 0 to be cast compatible}}
  %0 = "tf.Case"(%arg0, %arg1) {branches = [@branch0, @branch1], is_stateless = false} : (tensor<i32>, tensor<*xf32>) -> tensor<2xf32>
  return %0 : tensor<2xf32>
}

// -----

func private @branch0(tensor<*xf32>) -> tensor<*xf32>
func private @branch1(tensor<*xf32>) -> tensor<3xf32>

func @testCaseResultNotCastCompatible(%arg0: tensor<i32>, %arg1: tensor<*xf32>) -> tensor<2xf32> {
  // expected-error @+1 {{'tf.Case' op branch #1 result type tensor<3xf32> is incompatible with result type tensor<2xf32> at index 0}}
  %0 = "tf.Case"(%arg0, %arg1) {branches = [@branch0, @branch1], is_stateless = false} : (tensor<i32>, tensor<*xf32>) -> tensor<2xf32>
  return %0 : tensor<2xf32>
}

// -----

func @testCaseRegionNoRegions(%arg0: tensor<i32>) {
  // expected-error @+1 {{expects to have at least 1 region}}
  "tf.CaseRegion"(%arg0) {is_stateless = false} : (tensor<i32>) -> ()
  return
}

// -----

func @testCaseRegionBadBranchIndicesShape(%arg0: tensor<8xi32>) {
  // expected-error @+1 {{expects 'branch_index' to be a scalar, but got 'tensor<8xi32>'}}
  "tf.CaseRegion"(%arg0) ( {
    "tf.Yield"() : () -> ()
  }) {is_stateless = false} : (tensor<8xi32>) -> ()
  return
}

// -----

func @testCaseRegionMismatchedNumResults(%arg0: tensor<i32>) {
  // expected-error @+1 {{'tf.CaseRegion' op branch #0 results (size = 0) should have the same number of values as results (size = 1)}}
  %1 = "tf.CaseRegion"(%arg0) ( {
    "tf.Yield"() : () -> ()
  }) {is_stateless = false} : (tensor<i32>) -> tensor<i1>
  return
}

// -----

func @testCaseRegionMismatchedResultTypes(%arg0: tensor<i32>, %arg1: tensor<f32>) {
  // expected-error @+1 {{'tf.CaseRegion' op branch #0 result type tensor<f32> is incompatible with result type tensor<i1> at index 0}}
  %1 = "tf.CaseRegion"(%arg0) ( {
    "tf.Yield"(%arg1) : (tensor<f32>) -> ()
  }) {is_stateless = false} : (tensor<i32>) -> tensor<i1>
  return
}

// -----

// Test valid tf.Cumsum
func @testCumsum(%arg: tensor<8x16xf32>, %axis: tensor<i32>) -> tensor<8x16xf32> {
  %0 = "tf.Cumsum"(%arg, %axis) : (tensor<8x16xf32>, tensor<i32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>
}

// -----

func @testCumprod(%arg: tensor<8x16xf32>, %axis: tensor<2xi32>) -> tensor<8x16xf32> {
  // expected-error @+1 {{requires scalar axis operand}}
  %0 = "tf.Cumprod"(%arg, %axis) : (tensor<8x16xf32>, tensor<2xi32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>
}

// -----

func @testCumprod(%arg: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %axis = constant dense<-3> : tensor<i32>
  // expected-error @+1 {{axis operand should be within range [-2, 2)}}
  %0 = "tf.Cumprod"(%arg, %axis) : (tensor<8x16xf32>, tensor<i32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>
}

// -----

func @testTile(%arg0: tensor<2x3x?xf32>) {
  %cst = constant dense <[2, 3, 4]> : tensor<3xi32>
  %0 = "tf.Tile"(%arg0, %cst) : (tensor<2x3x?xf32>, tensor<3xi32>) -> tensor<4x9x?xf32>
  return
}

// -----

func @testTileMultipleNotRank1(%arg0: tensor<2x3xf32>, %arg1: tensor<1x1xi32>) {
  // expected-error @+1 {{expected multiples to be rank 1, got rank = 2}}
  %0 = "tf.Tile"(%arg0, %arg1) : (tensor<2x3xf32>, tensor<1x1xi32>) -> tensor<2x3xf32>
  return
}

// -----

func @testTileInputRankNotEqualToMultiplesSize(%arg0: tensor<2x3xf32>, %arg1: tensor<3xi32>) {
  // expected-error @+1 {{expected size of multiples equal to rank of input, got multiples of size 3, and input of rank 2}}
  %0 = "tf.Tile"(%arg0, %arg1) : (tensor<2x3xf32>, tensor<3xi32>) -> tensor<2x3xf32>
  return
}

// -----

func @testTileInputRankNotEqualToOutputRank(%arg0: tensor<2x3xf32>, %arg1: tensor<2xi32>) {
  // expected-error @+1 {{expected rank of input to equal to rank of output, got input of rank 2, and output of rank 3}}
  %0 = "tf.Tile"(%arg0, %arg1) : (tensor<2x3xf32>, tensor<2xi32>) -> tensor<2x3x1xf32>
  return
}

// -----

func @testTileNegativeMultiples(%arg0: tensor<2x3xf32>) {
  %cst = constant dense <[-1, 1]> : tensor<2xi32>
  // expected-error @+1 {{expected multiples to be non-negative, got multiples[0] = -1}}
  %0 = "tf.Tile"(%arg0, %cst) : (tensor<2x3xf32>, tensor<2xi32>) -> tensor<2x3xf32>
  return
}

// -----

func @testTileInvalidOutputShape(%arg0: tensor<2x3xf32>) {
  %cst = constant dense <[2, 3]> : tensor<2xi32>
  // expected-error @+1 {{requires input.shape[1] (3) * 3 to be equal to output.shape[1] (6)}}
  %0 = "tf.Tile"(%arg0, %cst) : (tensor<2x3xf32>, tensor<2xi32>) -> tensor<4x6xf32>
  return
}

// -----

// Test reference variable support for some ops (no errors expected)

// CHECK-LABEL: @testMaximumWithRef
func @testMaximumWithRef(%arg0: tensor<!tf.f32ref>, %arg1: tensor<f32>) -> tensor<f32> {
  // CHECK: tf.Maximum
  %0 = "tf.Maximum"(%arg0, %arg1) : (tensor<!tf.f32ref>, tensor<f32>) -> tensor<f32>
  return %0 : tensor<f32>
}

// CHECK-LABEL: @testAddV2WithRef
func @testAddV2WithRef(%arg0: tensor<!tf.int16ref>, %arg1: tensor<i16>) -> tensor<i16> {
  // CHECK: tf.AddV2
  %0 = "tf.AddV2"(%arg0, %arg1) : (tensor<!tf.int16ref>, tensor<i16>) -> tensor<i16>
  return %0 : tensor<i16>
}

// CHECK-LABEL: @testRealDivWithRef
func @testRealDivWithRef(%arg0: tensor<f64>, %arg1: tensor<!tf.f64ref>) -> tensor<f64> {
  // CHECK: tf.RealDivOp
  %0 = "tf.RealDivOp"(%arg0, %arg1) : (tensor<f64>, tensor<!tf.f64ref>) -> tensor<f64>
  return %0 : tensor<f64>
}

// CHECK-LABEL: @testDivNoNanWithRef
func @testDivNoNanWithRef(%arg0: tensor<f32>, %arg1: tensor<!tf.f32ref>) -> tensor<f32> {
  // CHECK: tf.DivNoNanOp
  %0 = "tf.DivNoNanOp"(%arg0, %arg1) : (tensor<f32>, tensor<!tf.f32ref>) -> tensor<f32>
  return %0 : tensor<f32>
}

// CHECK-LABEL: @testAddWithRef
func @testAddWithRef(%arg0: tensor<!tf.f64ref>, %arg1: tensor<f64>) -> tensor<f64> {
  // CHECK: tf.Add
  %0 = "tf.Add"(%arg0, %arg1) : (tensor<!tf.f64ref>, tensor<f64>) -> tensor<f64>
  return %0 : tensor<f64>
}

// -----

func @testInvalidTPUExecuteAndUpdateVariables(%arg0: tensor<!tf.resource<tensor<i32>>>, %arg1: tensor<3x!tf.string>) {
  // expected-error@below {{requires 'device_var_reads_indices' to be the same size as number of resource handles in 'args' (1), but got 2}}
  "tf.TPUExecuteAndUpdateVariables"(%arg0, %arg1) {device_var_reads_indices = [0, 1], device_var_updates_indices = [0]} : (tensor<!tf.resource<tensor<i32>>>, tensor<3x!tf.string>) -> ()
  return
}

// -----

func @testInvalidTPUExecuteAndUpdateVariables(%arg0: tensor<!tf.resource<tensor<i32>>>, %arg1: tensor<3x!tf.string>) {
  // expected-error@below {{requires 'device_var_updates_indices' to be the same size as number of resource handles in 'args' (1), but got 2}}
  "tf.TPUExecuteAndUpdateVariables"(%arg0, %arg1) {device_var_reads_indices = [0], device_var_updates_indices = [0, 1]} : (tensor<!tf.resource<tensor<i32>>>, tensor<3x!tf.string>) -> ()
  return
}

// -----

func @testInvalidTPUExecuteAndUpdateVariables(%arg0: tensor<!tf.resource<tensor<i32>>>, %arg1: tensor<3x!tf.string>) {
  // expected-error@below {{requires 'device_var_reads_indices' to contain values of at least 0, but got -1 at index 0}}
  "tf.TPUExecuteAndUpdateVariables"(%arg0, %arg1) {device_var_reads_indices = [-1], device_var_updates_indices = [0]} : (tensor<!tf.resource<tensor<i32>>>, tensor<3x!tf.string>) -> ()
  return
}

// -----

func @testInvalidTPUExecuteAndUpdateVariables(%arg0: tensor<!tf.resource<tensor<i32>>>, %arg1: tensor<3x!tf.string>) {
  // expected-error@below {{requires 'device_var_updates_indices' to contain values of at least -1, but got -2 at index 0}}
  "tf.TPUExecuteAndUpdateVariables"(%arg0, %arg1) {device_var_reads_indices = [0], device_var_updates_indices = [-2]} : (tensor<!tf.resource<tensor<i32>>>, tensor<3x!tf.string>) -> ()
  return
}

// -----

// Valid VarHandleOp operation.
// CHECK-LABEL: func @testVarHandleOp
func @testVarHandleOp() -> tensor<!tf.resource<tensor<*xf32>>> {
  %0 = "tf.VarHandleOp"() {
    container = "",
    shared_name = "cd2c89b7-88b7-44c8-ad83-06c2a9158347"
  } : () -> tensor<!tf.resource<tensor<*xf32>>>
  return %0 : tensor<!tf.resource<tensor<*xf32>>>
}

// -----

// VarHandleOp operation missing the required resource subtype.
func @testVarHandleOp() -> tensor<*x!tf.resource> {
  // expected-error @+1 {{must have exactly one subtype in the result resource type}}
  %0 = "tf.VarHandleOp"() {
    container = "",
    shared_name = "cd2c89b7-88b7-44c8-ad83-06c2a9158347"
  } : () -> tensor<*x!tf.resource>
  return %0 : tensor<*x!tf.resource>
}

// -----

func @testXlaBroadcastHelper(%arg0: tensor<2x3x5xi32>, %arg1: tensor<5x2xi32>) -> () {
  %0 = "tf.Const"() {value = dense<2> : tensor<1xi64>} : () -> tensor<1xi64>
  // expected-error @+1 {{broadcast_dims must have size equal to the smaller argument rank}}
  %lhs_output, %rhs_output = "tf.XlaBroadcastHelper"(%arg0, %arg1, %0) : (tensor<2x3x5xi32>, tensor<5x2xi32>, tensor<1xi64>) -> (tensor<2x3x5xi32>, tensor<2x1x5xi32>)
  return
}

// -----

func @testXlaBroadcastHelper(%arg0: tensor<2x3x5xi32>, %arg1: tensor<5x2xi32>) -> () {
  %0 = "tf.Const"() {value = dense<> : tensor<0xi64>} : () -> tensor<0xi64>
  // expected-error @+1 {{if broadcast_dims is empty, both arguments must have equal rank or at least one argument must be a scalar}}
  %lhs_output, %rhs_output = "tf.XlaBroadcastHelper"(%arg0, %arg1, %0) : (tensor<2x3x5xi32>, tensor<5x2xi32>, tensor<0xi64>) -> (tensor<2x3x5xi32>, tensor<2x1x5xi32>)
  return
}

// -----

func @testXlaBroadcastHelper(%arg0: tensor<5x2xi32>, %arg1: tensor<2x3x5xi32>) -> () {
  %0 = "tf.Const"() {value = dense<0> : tensor<2xi64>} : () -> tensor<2xi64>
  // expected-error @+1 {{broadcast_dims has duplicates}}
  %lhs_output, %rhs_output = "tf.XlaBroadcastHelper"(%arg0, %arg1, %0) : (tensor<5x2xi32>, tensor<2x3x5xi32>, tensor<2xi64>) -> (tensor<2x1x5xi32>, tensor<2x3x5xi32>)
  return
}

// -----

func @testXlaBroadcastHelper(%arg0: tensor<2xi32>, %arg1: tensor<i32>) -> () {
  %0 = "tf.Const"() {value = dense<> : tensor<0xi64>} : () -> tensor<0xi64>
  %lhs_output, %rhs_output = "tf.XlaBroadcastHelper"(%arg0, %arg1, %0) : (tensor<2xi32>, tensor<i32>, tensor<0xi64>) -> (tensor<2xi32>, tensor<i32>)
  return
}

// -----

func @testXlaBroadcastHelper(%arg0: tensor<5x2xi32>, %arg1: tensor<2x3x5xi32>) -> () {
  %0 = "tf.Const"() {value = dense<[2, 0]> : tensor<2xi64>} : () -> tensor<2xi64>
  %lhs_output, %rhs_output = "tf.XlaBroadcastHelper"(%arg0, %arg1, %0) : (tensor<5x2xi32>, tensor<2x3x5xi32>, tensor<2xi64>) -> (tensor<2x1x5xi32>, tensor<2x3x5xi32>)
  return
}

// -----

func @testXlaBroadcastHelper(%arg0: tensor<2x3x5xi32>, %arg1: tensor<5x2xi32>) -> () {
  %0 = "tf.Const"() {value = dense<[2, 0]> : tensor<2xi64>} : () -> tensor<2xi64>
  %lhs_output, %rhs_output = "tf.XlaBroadcastHelper"(%arg0, %arg1, %0) : (tensor<2x3x5xi32>, tensor<5x2xi32>, tensor<2xi64>) -> (tensor<2x3x5xi32>, tensor<2x1x5xi32>)
  return
}
