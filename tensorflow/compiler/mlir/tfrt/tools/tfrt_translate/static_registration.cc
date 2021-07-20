/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Translation.h"
#include "tfrt/bef_converter/bef_to_mlir_translate.h"
#include "tfrt/bef_converter/mlir_to_bef_translate.h"
#include "tfrt/init_tfrt_dialects.h"
#include "tensorflow/compiler/mlir/tfrt/jit/tf_cpurt_registration.h"

static mlir::TranslateFromMLIRRegistration mlir_to_bef_registration(
    "mlir-to-bef", tfrt::MLIRToBEFTranslate,
    [](mlir::DialectRegistry &registry) {
      tfrt::RegisterTFRTDialects(registry);
      tfrt::RegisterTFRTCompiledDialects(registry);
      tensorflow::RegisterTfCpurtDialect(registry);
    });

static mlir::TranslateToMLIRRegistration bef_to_mlir_registration(
    "bef-to-mlir", [](llvm::SourceMgr &source_mgr, mlir::MLIRContext *context) {
      mlir::DialectRegistry registry;
      tensorflow::RegisterTfCpurtDialect(registry);
      context->appendDialectRegistry(registry);
      return tfrt::BEFToMLIRTranslate(source_mgr, context);
    });
