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
#ifndef TENSORFLOW_LITE_C_C_API_EXPERIMENTAL_H_
#define TENSORFLOW_LITE_C_C_API_EXPERIMENTAL_H_

#include "tensorflow/lite/builtin_ops.h"
#include "tensorflow/lite/c/c_api.h"
#include "tensorflow/lite/c/common.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Resets all variable tensors to zero.
TFL_CAPI_EXPORT extern TfLiteStatus TfLiteInterpreterResetVariableTensors(
    TfLiteInterpreter* interpreter);

// Adds an op registration for a builtin operator.
//
// NOTE: The interpreter will make a copy of `registration` internally, so the
// caller should ensure that its contents (function pointers, etc...) remain
// valid for the duration of the interpreter's lifetime. A common practice is
// making the provided TfLiteRegistration instance static.
TFL_CAPI_EXPORT void TfLiteInterpreterOptionsAddBuiltinOp(
    TfLiteInterpreterOptions* options, TfLiteBuiltinOperator op,
    const TfLiteRegistration* registration, int32_t min_version,
    int32_t max_version);

// Returns a new interpreter using the provided model and options, or null on
// failure, where the model uses only the builtin operators specified in the
// options.  This is the same as `TFLiteInterpreterCreate` from `c_api.h`,
// except that the only builtin operators that are supported are the ones
// registered in `options` with `TfLiteInterpreterOptionsAddBuiltinOp`.
//
// * `model` must be a valid model instance. The caller retains ownership of the
//   object, and can destroy it immediately after creating the interpreter; the
//   interpreter will maintain its own reference to the underlying model data.
// * `options` should not be null. The caller retains ownership of the object,
//   and can safely destroy it immediately after creating the interpreter.
//
// NOTE: The client *must* explicitly allocate tensors before attempting to
// access input tensor data or invoke the interpreter.
TFL_CAPI_EXPORT extern TfLiteInterpreter*
TfLiteInterpreterCreateWithSelectedOps(const TfLiteModel* model,
                                       const TfLiteInterpreterOptions* options);

// Adds an op registration for a custom operator.
//
// NOTE: The interpreter will make a copy of `registration` internally, so the
// caller should ensure that its contents (function pointers, etc...) remain
// valid for the duration of any created interpreter's lifetime. A common
// practice is making the provided TfLiteRegistration instance static.
TFL_CAPI_EXPORT void TfLiteInterpreterOptionsAddCustomOp(
    TfLiteInterpreterOptions* options, const char* name,
    const TfLiteRegistration* registration, int32_t min_version,
    int32_t max_version);

// Enable or disable the NN API for the interpreter (true to enable).
TFL_CAPI_EXPORT extern void TfLiteInterpreterOptionsSetUseNNAPI(
    TfLiteInterpreterOptions* options, bool enable);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // TENSORFLOW_LITE_C_C_API_EXPERIMENTAL_H_
