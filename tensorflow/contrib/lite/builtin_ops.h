/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_CONTRIB_LITE_BUILTIN_OPS_H_
#define TENSORFLOW_CONTRIB_LITE_BUILTIN_OPS_H_

// DO NOT EDIT MANUALLY: This file is automatically generated by
// `schema/builtin_ops_header/generator.cc`.

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// The enum for builtin operators.
// Note: CUSTOM and DELEGATE are 2 special ops which are not real built-in ops.
typedef enum {
  kTfLiteBuiltinAdd = 0,
  kTfLiteBuiltinAveragePool2d = 1,
  kTfLiteBuiltinConcatenation = 2,
  kTfLiteBuiltinConv2d = 3,
  kTfLiteBuiltinDepthwiseConv2d = 4,
  kTfLiteBuiltinDequantize = 6,
  kTfLiteBuiltinEmbeddingLookup = 7,
  kTfLiteBuiltinFloor = 8,
  kTfLiteBuiltinFullyConnected = 9,
  kTfLiteBuiltinHashtableLookup = 10,
  kTfLiteBuiltinL2Normalization = 11,
  kTfLiteBuiltinL2Pool2d = 12,
  kTfLiteBuiltinLocalResponseNormalization = 13,
  kTfLiteBuiltinLogistic = 14,
  kTfLiteBuiltinLshProjection = 15,
  kTfLiteBuiltinLstm = 16,
  kTfLiteBuiltinMaxPool2d = 17,
  kTfLiteBuiltinMul = 18,
  kTfLiteBuiltinRelu = 19,
  kTfLiteBuiltinReluN1To1 = 20,
  kTfLiteBuiltinRelu6 = 21,
  kTfLiteBuiltinReshape = 22,
  kTfLiteBuiltinResizeBilinear = 23,
  kTfLiteBuiltinRnn = 24,
  kTfLiteBuiltinSoftmax = 25,
  kTfLiteBuiltinSpaceToDepth = 26,
  kTfLiteBuiltinSvdf = 27,
  kTfLiteBuiltinTanh = 28,
  kTfLiteBuiltinConcatEmbeddings = 29,
  kTfLiteBuiltinSkipGram = 30,
  kTfLiteBuiltinCall = 31,
  kTfLiteBuiltinCustom = 32,
  kTfLiteBuiltinEmbeddingLookupSparse = 33,
  kTfLiteBuiltinPad = 34,
  kTfLiteBuiltinUnidirectionalSequenceRnn = 35,
  kTfLiteBuiltinGather = 36,
  kTfLiteBuiltinBatchToSpaceNd = 37,
  kTfLiteBuiltinSpaceToBatchNd = 38,
  kTfLiteBuiltinTranspose = 39,
  kTfLiteBuiltinMean = 40,
  kTfLiteBuiltinSub = 41,
  kTfLiteBuiltinDiv = 42,
  kTfLiteBuiltinSqueeze = 43,
  kTfLiteBuiltinUnidirectionalSequenceLstm = 44,
  kTfLiteBuiltinStridedSlice = 45,
  kTfLiteBuiltinBidirectionalSequenceRnn = 46,
  kTfLiteBuiltinExp = 47,
  kTfLiteBuiltinTopkV2 = 48,
  kTfLiteBuiltinSplit = 49,
  kTfLiteBuiltinLogSoftmax = 50,
  kTfLiteBuiltinDelegate = 51,
  kTfLiteBuiltinBidirectionalSequenceLstm = 52,
  kTfLiteBuiltinCast = 53,
  kTfLiteBuiltinPrelu = 54,
  kTfLiteBuiltinMaximum = 55,
  kTfLiteBuiltinArgMax = 56,
  kTfLiteBuiltinMinimum = 57,
  kTfLiteBuiltinLess = 58,
  kTfLiteBuiltinNeg = 59,
  kTfLiteBuiltinPadv2 = 60,
  kTfLiteBuiltinGreater = 61,
  kTfLiteBuiltinGreaterEqual = 62,
  kTfLiteBuiltinLessEqual = 63,
  kTfLiteBuiltinSelect = 64,
  kTfLiteBuiltinSlice = 65,
  kTfLiteBuiltinSin = 66,
  kTfLiteBuiltinTransposeConv = 67,
  kTfLiteBuiltinSparseToDense = 68,
  kTfLiteBuiltinTile = 69,
  kTfLiteBuiltinExpandDims = 70,
  kTfLiteBuiltinEqual = 71,
  kTfLiteBuiltinNotEqual = 72,
  kTfLiteBuiltinLog = 73,
  kTfLiteBuiltinSum = 74,
  kTfLiteBuiltinSqrt = 75,
  kTfLiteBuiltinRsqrt = 76,
  kTfLiteBuiltinShape = 77,
  kTfLiteBuiltinPow = 78,
  kTfLiteBuiltinArgMin = 79,
  kTfLiteBuiltinFakeQuant = 80,
  kTfLiteBuiltinReduceProd = 81,
  kTfLiteBuiltinReduceMax = 82,
  kTfLiteBuiltinPack = 83,
  kTfLiteBuiltinLogicalOr = 84,
  kTfLiteBuiltinOneHot = 85,
  kTfLiteBuiltinLogicalAnd = 86,
  kTfLiteBuiltinLogicalNot = 87,
  kTfLiteBuiltinUnpack = 88,
  kTfLiteBuiltinReduceMin = 89,
  kTfLiteBuiltinFloorDiv = 90,
  kTfLiteBuiltinReduceAny = 91,
} TfLiteBuiltinOperator;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // TENSORFLOW_CONTRIB_LITE_BUILTIN_OPS_H_
