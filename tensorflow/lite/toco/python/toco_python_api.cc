/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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
#include <map>
#include <string>
#include <vector>

#include "tensorflow/core/platform/logging.h"
#include "tensorflow/lite/python/interpreter_wrapper/python_utils.h"
#include "tensorflow/lite/toco/import_tensorflow.h"
#include "tensorflow/lite/toco/model_flags.pb.h"
#include "tensorflow/lite/toco/python/toco_python_api.h"
#include "tensorflow/lite/toco/toco_flags.pb.h"
#include "tensorflow/lite/toco/toco_graphviz_dump_options.h"
#include "tensorflow/lite/toco/toco_port.h"
#include "tensorflow/lite/toco/toco_tooling.h"
#include "tensorflow/lite/toco/toco_types.h"

namespace toco {

// NOTE(aselle): We are using raw PyObject's here because we want to make
// sure we input and output bytes rather than unicode strings for Python3.
PyObject* TocoConvert(PyObject* model_flags_proto_txt_raw,
                      PyObject* toco_flags_proto_txt_raw,
                      PyObject* input_contents_txt_raw, bool extended_return) {
  // Use Python C API to validate and convert arguments. In py3 (bytes),
  // in py2 (str).
  auto ConvertArg = [&](PyObject* obj, bool* error) {
    char* buf;
    Py_ssize_t len;
    if (::tflite::python_utils::ConvertFromPyString(obj, &buf, &len) == -1) {
      *error = true;
      return std::string();
    } else {
      *error = false;
      return std::string(buf, len);
    }
  };

  bool error;
  std::string model_flags_proto_txt =
      ConvertArg(model_flags_proto_txt_raw, &error);
  if (error) {
    PyErr_SetString(PyExc_ValueError, "Model flags are invalid.");
    return nullptr;
  }
  std::string toco_flags_proto_txt =
      ConvertArg(toco_flags_proto_txt_raw, &error);
  if (error) {
    PyErr_SetString(PyExc_ValueError, "Toco flags are invalid.");
    return nullptr;
  }
  std::string input_contents_txt = ConvertArg(input_contents_txt_raw, &error);
  if (error) {
    PyErr_SetString(PyExc_ValueError, "Input GraphDef is invalid.");
    return nullptr;
  }

  // Use TOCO to produce new outputs.
  toco::ModelFlags model_flags;
  if (!model_flags.ParseFromString(model_flags_proto_txt)) {
    PyErr_SetString(PyExc_ValueError, "Model proto failed to parse.");
    return nullptr;
  }
  toco::TocoFlags toco_flags;
  if (!toco_flags.ParseFromString(toco_flags_proto_txt)) {
    PyErr_SetString(PyExc_ValueError, "Toco proto failed to parse.");
    return nullptr;
  }

  auto& dump_options = *GraphVizDumpOptions::singleton();
  if (toco_flags.has_dump_graphviz_dir()) {
    dump_options.dump_graphviz = toco_flags.dump_graphviz_dir();
  }
  if (toco_flags.has_dump_graphviz_include_video()) {
    dump_options.dump_graphviz_video = toco_flags.dump_graphviz_include_video();
  }

  // Convert model.
  std::unique_ptr<toco::Model> model =
      toco::Import(toco_flags, model_flags, input_contents_txt);
  toco::Transform(toco_flags, model.get());
  string output_file_contents_txt;
  auto status = Export(toco_flags, *model, toco_flags.allow_custom_ops(),
                       &output_file_contents_txt);
  if (!status.ok()) {
    PyErr_SetString(PyExc_Exception, status.error_message().c_str());
    return nullptr;
  }
  if (extended_return) {
    PyObject* dict = PyDict_New();
    PyDict_SetItemString(
        dict, "flatbuffer",
        ::tflite::python_utils::ConvertToPyString(
            output_file_contents_txt.data(), output_file_contents_txt.size()));
    PyDict_SetItemString(dict, "arithmetic_ops",
                         PyLong_FromLong(model->ArithmeticOpsCount()));
    return dict;
  }
  // Convert arguments back to byte (py3) or str (py2)
  return ::tflite::python_utils::ConvertToPyString(
      output_file_contents_txt.data(), output_file_contents_txt.size());
}

PyObject* TocoGetPotentiallySupportedOps() {
  std::vector<std::string> supported_ops = toco::GetPotentiallySupportedOps();
  PyObject* list = PyList_New(supported_ops.size());
  for (size_t i = 0; i < supported_ops.size(); ++i) {
    const string& op = supported_ops[i];
    PyObject* op_dict = PyDict_New();
    PyDict_SetItemString(op_dict, "op", PyUnicode_FromString(op.c_str()));
    PyList_SetItem(list, i, op_dict);
  }
  return list;
}

}  // namespace toco
