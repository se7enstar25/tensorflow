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
#include <ctype.h>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include "flatbuffers/minireflect.h"  // from @flatbuffers
#include "tensorflow/lite/schema/reflection/schema_generated.h"

namespace tflite {
namespace {
// This is generated by grepping
//  cat  third_party/tensorflow/lite/c/builtin_op_data.h | grep "^} TfLite" |
//  sed 's/^} \(TfLite.*\)Params;/\1Params/g' | grep -v "^}" | sed
//  's/\(.*\)/"\1",/g' | sort
static const char* param_structs[] = {"TfLiteAddParams",
                                      "TfLiteArgMaxParams",
                                      "TfLiteArgMinParams",
                                      "TfLiteBatchMatMulParams",
                                      "TfLiteBatchToSpaceNDParams",
                                      "TfLiteBidirectionalSequenceLSTMParams",
                                      "TfLiteBidirectionalSequenceRNNParams",
                                      "TfLiteCastParams",
                                      "TfLiteConcatenationParams",
                                      "TfLiteConvParams",
                                      "TfLiteDepthwiseConvParams",
                                      "TfLiteDivParams",
                                      "TfLiteEmbeddingLookupSparseParams",
                                      "TfLiteFakeQuantParams",
                                      "TfLiteFullyConnectedParams",
                                      "TfLiteGatherParams",
                                      "TfLiteIfParams",
                                      "TfLiteL2NormParams",
                                      "TfLiteLeakyReluParams",
                                      "TfLiteLocalResponseNormParams",
                                      "TfLiteLSHProjectionParams",
                                      "TfLiteLSTMParams",
                                      "TfLiteMirrorPaddingParams",
                                      "TfLiteMulParams",
                                      "TfLiteOneHotParams",
                                      "TfLitePackParams",
                                      "TfLitePadParams",
                                      "TfLitePadV2Params",
                                      "TfLitePoolParams",
                                      "TfLiteReducerParams",
                                      "TfLiteReshapeParams",
                                      "TfLiteResizeBilinearParams",
                                      "TfLiteResizeNearestNeighborParams",
                                      "TfLiteRNNParams",
                                      "TfLiteSequenceRNNParams",
                                      "TfLiteShapeParams",
                                      "TfLiteSkipGramParams",
                                      "TfLiteSoftmaxParams",
                                      "TfLiteSpaceToBatchNDParams",
                                      "TfLiteSpaceToDepthParams",
                                      "TfLiteDepthToSpaceParams",
                                      "TfLiteSparseToDenseParams",
                                      "TfLiteSplitParams",
                                      "TfLiteSplitVParams",
                                      "TfLiteSqueezeParams",
                                      "TfLiteStridedSliceParams",
                                      "TfLiteSubParams",
                                      "TfLiteSVDFParams",
                                      "TfLiteTransposeConvParams",
                                      "TfLiteTransposeParams",
                                      "TfLiteUnidirectionalSequenceLSTMParams",
                                      "TfLiteUniqueParams",
                                      "TfLiteUnpackParams",
                                      "TfLiteReverseSequenceParams",
                                      "TfLiteWhileParams",
                                      nullptr};
}  // namespace

// Get rid of all underscores and make everything lower case to make name
// matching work for stuff like 3D vs 3d or RNN vs Rnn.
std::string ToCollapsed(const std::string& in) {
  const char* s = in.c_str();
  bool first = true;
  std::string out;
  while (*s != '\0') {
    if (*s == '_') {
      first = true;
    } else if (first) {
      out.push_back(tolower(*s));
      first = false;
    } else {
      out.push_back(tolower(*s));
    }
    s++;
  }
  return out;
}

// A collection of information about builtin ops.
class OpOptionData {
 public:
  OpOptionData() {
    BuildOpList();
    BuildOptionToTypeFunctionMap();
    BuildOpToOptionMap();
  }

  // A list of builtin operations
  const std::vector<std::string>& ops() const { return ops_; }
  // Maps from operation name to option name (i.e. 'ADD' to 'AddOptions')
  const std::unordered_map<std::string, std::string>& op_to_option() {
    return op_to_option_;
  }
  // Maps from option to to C struct i.e. 'AddOptions' -> 'TfLiteAddOptions'
  const std::unordered_map<std::string, std::string>& option_to_struct() {
    return option_to_struct_;
  }
  // Maps from option to a flatbuffer type function that describes that option.
  const std::unordered_map<std::string, flatbuffers::TypeFunction>&
  option_to_type_function() {
    return option_to_type_function_;
  }

 private:
  void BuildOpList() {
    for (const char* const* curr = EnumNamesBuiltinOperator(); *curr != nullptr;
         ++curr) {
      if (strlen(*curr) != 0) ops_.push_back(*curr);
    }
  }

  void BuildOptionToTypeFunctionMap() {
    auto d = tflite::BuiltinOptionsTypeTable();
    for (int i = 0; i < d->num_elems; i++) {
      flatbuffers::TypeCode code = d->type_codes[i];
      if (code.sequence_ref != -1) {
        option_to_type_function_.insert(
            std::make_pair(d->names[i], d->type_refs[code.sequence_ref]));
      }
    }
  }

  void BuildOpToOptionMap() {
    // Manually specified mappings between ops and options
    op_to_option_["REDUCE_MAX"] = "ReducerOptions";
    op_to_option_["REDUCE_MIN"] = "ReducerOptions";
    op_to_option_["REDUCE_ANY"] = "ReducerOptions";
    op_to_option_["SUM"] = "ReducerOptions";
    op_to_option_["REDUCE_MAX"] = "ReducerOptions";
    op_to_option_["REDUCE_PROD"] = "ReducerOptions";
    op_to_option_["MEAN"] = "ReducerOptions";
    op_to_option_["L2_POOL_2D"] = "Pool2DOptions";
    op_to_option_["AVERAGE_POOL_2D"] = "Pool2DOptions";
    op_to_option_["MAX_POOL_2D"] = "Pool2DOptions";
    op_to_option_["L2_NORMALIZATION"] = "L2NormOptions";
    op_to_option_["UNIDIRECTIONAL_SEQUENCE_RNN"] = "SequenceRNNOptions";
    op_to_option_["MAXIMUM"] = "MaximumMinimumOptions";
    op_to_option_["MINIMUM"] = "MaximumMinimumOptions";
    op_to_option_["CUSTOM"] = "";    // TODO(aselle): maybe something else.
    op_to_option_["DELEGATE"] = "";  // TODO(aselle): maybe something else.

    // Manually specified mappings between ops to "none" options -- these are
    // ops without a corresponding Options message in schema as yet. If these
    // options do get assigned an Options message in future, they need to be
    // updated here as well.
    op_to_option_["EMBEDDING_LOOKUP"] = "";
    op_to_option_["FLOOR"] = "";
    op_to_option_["CEIL"] = "";
    op_to_option_["HASHTABLE_LOOKUP"] = "";
    op_to_option_["LOGISTIC"] = "";
    op_to_option_["RELU"] = "";
    op_to_option_["RELU_N1_TO_1"] = "";
    op_to_option_["RELU6"] = "";
    op_to_option_["ROUND"] = "";
    op_to_option_["TANH"] = "";
    op_to_option_["PRELU"] = "";
    op_to_option_["SIN"] = "";
    op_to_option_["LOG"] = "";
    op_to_option_["SQRT"] = "";
    op_to_option_["RSQRT"] = "";
    op_to_option_["ELU"] = "";
    op_to_option_["REVERSE_SEQUENCE"] = "";

    // TODO(aselle): These are undesirable hacks. Consider changing C structs
    option_to_struct_["Pool2DOptions"] = "TfLitePoolParams";
    option_to_struct_["Conv2DOptions"] = "TfLiteConvParams";
    option_to_struct_["DepthwiseConv2DOptions"] = "TfLiteDepthwiseConvParams";
    option_to_struct_["LocalResponseNormalizationOptions"] =
        "TfLiteLocalResponseNormParams";
    option_to_struct_["MirrorPadOptions"] = "TfLiteMirrorPaddingParams";
    // Now for every op, try to find an option.
    bool fatal = false;
    for (const auto& op_name : ops_) {
      bool found_option = false;
      auto d = tflite::BuiltinOptionsTypeTable();
      std::string collapsed_option_name_guess =
          ToCollapsed(op_name) + "options";
      // O(n^2) but not that big of n.
      for (int i = 0; i < d->num_elems; i++) {
        std::string option_name = d->names[i];
        std::string collapsed_option_name = ToCollapsed(option_name);
        if (collapsed_option_name_guess == collapsed_option_name) {
          op_to_option_.insert(std::make_pair(op_name, option_name));
          found_option = true;
          break;
        }
      }
      auto it = op_to_option_.find(op_name);
      if (it == op_to_option_.end()) {
        std::cerr << "Didn't find option for  " << op_name << std::endl;
        fatal = true;
      } else if (!it->second.empty()) {
        std::string option_name = it->second;

        if (option_to_struct_.find(option_name) == option_to_struct_.end()) {
          bool param_struct_found = false;
          std::string params_guess = std::string("TfLite") + option_name;
          size_t start = params_guess.find("Options");
          size_t len = strlen("Options");
          params_guess.replace(start, len, "Params");
          for (auto* param = param_structs; *param != nullptr; param++) {
            if (*param == params_guess) {
              param_struct_found = true;
              break;
            }
          }
          if (!param_struct_found) {
            std::cerr << "Failed to get param struct for option " << option_name
                      << std::endl;
          } else {
            option_to_struct_.insert(std::make_pair(option_name, params_guess));
          }
        }
      }
    }
    if (fatal) {
      exit(1);
    }
  }

 private:
  std::vector<std::string> ops_;
  std::unordered_map<std::string, std::string> op_to_option_;
  std::unordered_map<std::string, std::string> option_to_struct_;
  std::unordered_map<std::string, flatbuffers::TypeFunction>
      option_to_type_function_;
};

void GenerateImportForResizeBilinearOp(FILE* fp) {
  fprintf(fp,
          "  case BuiltinOperator_RESIZE_BILINEAR:  {\n"
          "    const auto* params = reinterpret_cast<const "
          "TfLiteResizeBilinearParams*>(builtin_op_data);\n"
          "    auto union_type = CreateResizeBilinearOptions(*fbb, "
          "params->align_corners, params->half_pixel_centers).Union();\n"
          "    return std::make_pair(BuiltinOptions_ResizeBilinearOptions, "
          "union_type);\n"
          "  }\n  break;\n");
}

void GenerateImportForOp(FILE* fp, const std::string& op_name,
                         const std::string& option_name,
                         const std::string& option_type,
                         const flatbuffers::TypeTable* options,
                         const std::string& struct_name) {
  // Special-case ResizeBilinear which has some deprecated fields.
  if (struct_name == "TfLiteResizeBilinearParams") {
    GenerateImportForResizeBilinearOp(fp);
    return;
  }

  fprintf(fp, "  case BuiltinOperator_%s:  {\n", op_name.c_str());
  if (options->num_elems != 0) {
    fprintf(fp,
            "    const auto* params = reinterpret_cast<const "
            "%s*>(builtin_op_data);\n",
            struct_name.c_str());
  }

  for (size_t i = 0; i < options->num_elems; i++) {
    std::string elem_name = options->names[i];
    bool is_int_vector = false;
    std::string vector_name = elem_name;
    std::string vector_size;
    // TODO(aselle): Irregular naming in builtins
    if (elem_name == "fused_activation_function")
      elem_name = "activation";
    else if (elem_name == "stride_w")
      elem_name = "stride_width";
    else if (elem_name == "stride_h")
      elem_name = "stride_height";
    else if (elem_name == "dilation_h_factor")
      elem_name = "dilation_height_factor";
    else if (elem_name == "dilation_w_factor")
      elem_name = "dilation_width_factor";
    else if (elem_name == "idx_out_type")
      elem_name = "index_out_type";

    // Vector fields treated specially.
    if (elem_name == "new_shape") {
      is_int_vector = true;
      vector_name = "shape";
      vector_size = "num_dimensions";
    } else if (elem_name == "squeeze_dims") {
      is_int_vector = true;
      vector_size = "num_squeeze_dims";
    }

    if (is_int_vector) {
      fprintf(fp,
              "    auto val%zu = fbb->CreateVector("
              "std::vector<int>(params->%s, params->%s + params->%s));\n",
              i, vector_name.c_str(), vector_name.c_str(), vector_size.c_str());
      continue;
    }

    flatbuffers::TypeCode code = options->type_codes[i];
    auto contained_type = code.sequence_ref != -1
                              ? options->type_refs[code.sequence_ref]
                              : nullptr;
    std::string mapper = "";
    if (contained_type == TensorTypeTypeTable) {
      mapper = "TfLiteTypeToSchemaType";
    } else if (contained_type == ActivationFunctionTypeTypeTable) {
      mapper = "TfLiteActivationToSchemaActivation";
    } else if (contained_type == PaddingTypeTable) {
      mapper = "TfLitePaddingToSchemaPadding";
    } else if (contained_type == FullyConnectedOptionsWeightsFormatTypeTable) {
      mapper = "FullyConnectedOptionsWeightsFormatToSchema";
    } else if (contained_type == LSTMKernelTypeTypeTable) {
      mapper = "LSTMKernelTypeToSchema";
    } else if (contained_type == LSHProjectionTypeTypeTable) {
      mapper = "LSHProjectionTypeToSchema";
    } else if (contained_type == MirrorPadModeTypeTable) {
      mapper = "MirrorPaddingModeToSchema";
    } else if (contained_type == CombinerTypeTypeTable) {
      mapper = "CombinerTypeToSchema";
    }

    fprintf(fp,
            "    auto val%zu = "
            "%s(params->%s);\n",
            i, mapper.c_str(), elem_name.c_str());
  }
  fprintf(fp, "    auto union_type = Create%s(*fbb", option_name.c_str());
  for (size_t i = 0; i < options->num_elems; i++) {
    fprintf(fp, ", val%zu", i);
  }
  fprintf(fp, ").Union();\n");
  fprintf(fp, "    return std::make_pair(%s, union_type);\n",
          option_type.c_str());
  fprintf(fp, "  }\n  break;\n");
}

void GenerateImport(OpOptionData* option, FILE* fp) {
  std::unordered_set<std::string> ignores;
  ignores.insert("CONCAT_EMBEDDINGS");
  ignores.insert("CALL");

  // Allow any op that doesn't have an options struct to be blocked
  // together
  for (const auto& op_name : option->ops()) {
    auto option_it = option->op_to_option().find(op_name);
    if (!option_it->second.empty() && ignores.find(op_name) == ignores.end())
      continue;
    fprintf(fp, "  case BuiltinOperator_%s:\n", op_name.c_str());
  }
  fprintf(fp,
          "    return std::make_pair(BuiltinOptions_NONE, "
          "flatbuffers::Offset<void>());\n    break;\n");

  // Iterate over each ops
  for (const auto& op_name : option->ops()) {
    if (ignores.find(op_name) != ignores.end()) continue;
    // Get to the option and struct names, continuing if not found.
    auto option_it = option->op_to_option().find(op_name);
    if (option_it->second.empty()) continue;
    std::string option_name = option_it->second;
    std::string option_type = "BuiltinOptions_" + option_name;
    auto option_func_it = option->option_to_type_function().find(option_name);
    if (option_func_it == option->option_to_type_function().end()) continue;
    auto struct_name_it = option->option_to_struct().find(option_name);
    if (struct_name_it == option->option_to_struct().end()) {
      // If no C struct, then it better have no arguments.
      auto type_info = option_func_it->second();
      if (type_info->num_elems != 0) {
        // We have non-zero arguments in the schema, this means there
        // should be a struct.
        fprintf(stderr,
                "Op %s uses option struct %s which has no builtin struct\n",
                op_name.c_str(), option_name.c_str());
        exit(1);
      }
      fprintf(fp, "  case BuiltinOperator_%s:\n", op_name.c_str());
      fprintf(fp, "    return std::make_pair(%s, Create%s(*fbb).Union());",
              option_type.c_str(), option_name.c_str());
    } else {
      // If C struct, then we need to assign all properties
      auto struct_name = struct_name_it->second;
      GenerateImportForOp(fp, op_name, option_name, option_type,
                          option_func_it->second(), struct_name);
    }
  }
  // TODO(aselle): Handle unhandled cases more gracefully.
  fprintf(fp,
          "default:    return std::make_pair(BuiltinOptions_NONE, "
          "flatbuffers::Offset<void>());\n    break;\n");
}

}  // namespace tflite

int main(int argc, char* argv[]) {
  tflite::OpOptionData option;
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <fname out>\n", argv[0]);
    return 1;
  }
  FILE* fp = fopen(argv[1], "w");
  tflite::GenerateImport(&option, fp);
  fclose(fp);
}
