// RUN: flatbuffer_translate -mlir-to-tflite-flatbuffer %s -o - | flatbuffer_to_string - | FileCheck --dump-input-on-failure %s

func @main(tensor<1x224x224x3xf32>) -> tensor<1x112x112x32xf32> {
^bb0(%arg0: tensor<1x224x224x3xf32>):
  // CHECK:      {
  // CHECK-NEXT:  version: 3,
  // CHECK-NEXT:  operator_codes: [ {
  // CHECK-NEXT:    builtin_code: DEQUANTIZE,
  // CHECK-NEXT:    version: 1
  // CHECK-NEXT:  }, {
  // CHECK-NEXT:    builtin_code: DEPTHWISE_CONV_2D,
  // CHECK-NEXT:    version: 1
  // CHECK-NEXT:  } ],
  // CHECK-NEXT:  subgraphs: [ {
  // CHECK-NEXT:    tensors: [ {
  // CHECK-NEXT:      shape: [ 1, 224, 224, 3 ],
  // CHECK-NEXT:      buffer: 1,
  // CHECK-NEXT:      name: "arg0",
  // CHECK-NEXT:      quantization: {
  // CHECK-EMPTY:
  // CHECK-NEXT:      }
  // CHECK-NEXT:    }, {
  // CHECK-NEXT:      shape: [ 32 ],
  // CHECK-NEXT:      buffer: 2,
  // CHECK-NEXT:      name: "Const",
  // CHECK-NEXT:      quantization: {
  // CHECK-EMPTY:
  // CHECK-NEXT:      }
  // CHECK-NEXT:    }, {
  // CHECK-NEXT:      shape: [ 32, 3, 3, 3 ],
  // CHECK-NEXT:      type: UINT8,
  // CHECK-NEXT:      buffer: 3,
  // CHECK-NEXT:      name: "tfl.pseudo_qconst",
  // CHECK-NEXT:      quantization: {
  // CHECK-NEXT:        scale: [ 0.021827 ],
  // CHECK-NEXT:        zero_point: [ 151 ]
  // CHECK-NEXT:      }
  // CHECK-NEXT:    }, {
  // CHECK-NEXT:      shape: [ 32, 3, 3, 3 ],
  // CHECK-NEXT:      buffer: 4,
  // CHECK-NEXT:      name: "tfl.dequantize",
  // CHECK-NEXT:      quantization: {
  // CHECK-EMPTY:
  // CHECK-NEXT:      }
  // CHECK-NEXT:    }, {
  // CHECK-NEXT:      shape: [ 1, 112, 112, 32 ],
  // CHECK-NEXT:      buffer: 5,
  // CHECK-NEXT:      name: "tfl.depthwise_conv_2d",
  // CHECK-NEXT:      quantization: {
  // CHECK-EMPTY:
  // CHECK-NEXT:      }
  // CHECK-NEXT:    } ],
  // CHECK-NEXT:    inputs: [ 0 ],
  // CHECK-NEXT:    outputs: [ 4 ],
  // CHECK-NEXT:    operators: [ {
  // CHECK-NEXT:      inputs: [ 2 ],
  // CHECK-NEXT:      outputs: [ 3 ]
  // CHECK-NEXT:    }, {
  // CHECK-NEXT:      opcode_index: 1,
  // CHECK-NEXT:      inputs: [ 0, 3, 1 ],
  // CHECK-NEXT:      outputs: [ 4 ],
  // CHECK-NEXT:      builtin_options_type: DepthwiseConv2DOptions,
  // CHECK-NEXT:      builtin_options: {
  // CHECK-NEXT:        padding: VALID,
  // CHECK-NEXT:        stride_w: 5,
  // CHECK-NEXT:        stride_h: 4,
  // CHECK-NEXT:        depth_multiplier: 4
  // CHECK-NEXT:      }
  // CHECK-NEXT:    } ],
  // CHECK-NEXT:    name: "main"
  // CHECK-NEXT:  } ],
  // CHECK-NEXT:  description: "MLIR Converted.",
  // CHECK-NEXT:  buffers: [ {
  // CHECK-EMPTY:
  // CHECK-NEXT:  }, {
  // CHECK-EMPTY:
  // CHECK-NEXT:  }, {
  // CHECK-NEXT:    data: [ 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191, 84, 85, 158, 191 ]
  // CHECK-NEXT:  }, {
  // CHECK-NEXT:    data: [ 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180 ]
  // CHECK-NEXT:  }, {
  // CHECK-EMPTY:
  // CHECK-NEXT:  }, {
  // CHECK-EMPTY:
  // CHECK-NEXT:  }, {
  // CHECK-NEXT:    data: [ 49, 46, 49, 51, 46, 49, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]
  // CHECK-NEXT:  } ],
  // CHECK-NEXT:  metadata: [ {
  // CHECK-NEXT:  name: "min_runtime_version",
  // CHECK-NEXT:  buffer: 6
  // CHECK-NEXT:  } ]
  // CHECK-NEXT:}

  %0 = "tfl.pseudo_const" () {value = dense<-1.23697901> : tensor<32xf32>} : () -> tensor<32xf32> loc("Const")
  %1 = "tfl.pseudo_qconst"() {qtype = tensor<32x3x3x3x!quant.uniform<u8<1:255>:f32, 0.021826678373682216:151>>, value = dense<-76> : tensor<32x3x3x3xi8>} : () -> tensor<32x3x3x3x!quant.uniform<u8<1:255>:f32, 0.021826678373682216:151>>
  %2 = "tfl.dequantize"(%1) : (tensor<32x3x3x3x!quant.uniform<u8<1:255>:f32, 0.021826678373682216:151>>) -> tensor<32x3x3x3xf32>
  %3 = "tfl.depthwise_conv_2d"(%arg0, %2, %0) {depth_multiplier = 4 : i32, dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<1x224x224x3xf32>, tensor<32x3x3x3xf32>, tensor<32xf32>) -> tensor<1x112x112x32xf32>

  return %3 : tensor<1x112x112x32xf32>
}
