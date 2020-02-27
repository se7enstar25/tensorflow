/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

#include "mbed.h"

#include "tensorflow/lite/micro/kernels/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

#include "tensorflow/lite/micro/examples/experimental/image_recognition/stm32f746_discovery/display_util.h"
#include "tensorflow/lite/micro/examples/experimental/image_recognition/stm32f746_discovery/image_util.h"
#include "tensorflow/lite/micro/examples/experimental/image_recognition/image_provider.h"
#include "tensorflow/lite/micro/examples/experimental/image_recognition/util.h"

#include "tensorflow/lite/micro/examples/experimental/image_recognition/image_recognition_model.h"

#define NUM_OUT_CH 3
#define CNN_IMG_SIZE 32

uint8_t camera_buffer[NUM_IN_CH * IN_IMG_WIDTH * IN_IMG_HEIGHT]
    __attribute__((aligned(4)));
static const char* labels[] = {"Plane", "Car",  "Bird", "Cat",
                               "Deer",  "Dog",  "Frog", "Horse",
                               "Ship",  "Truck"};

int main(int argc, char** argv) {
  init_lcd();
  wait_ms(100);

  tflite::MicroErrorReporter micro_error_reporter;
  tflite::ErrorReporter* error_reporter = &micro_error_reporter;

  if (InitCamera(error_reporter) != kTfLiteOk) {
    error_reporter->Report("Failed to init camera.");
    return 1;
  }

  const tflite::Model* model = ::tflite::GetModel(image_recognition_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report(
        "Model provided is schema version %d not equal "
        "to supported version %d.",
        model->version(), TFLITE_SCHEMA_VERSION);
    return 1;
  }

  tflite::ops::micro::AllOpsResolver resolver;

  constexpr int tensor_arena_size = 63 * 1024;
  uint8_t tensor_arena[tensor_arena_size];
  tflite::MicroInterpreter interpreter(model, resolver, tensor_arena,
                                       tensor_arena_size, error_reporter);
  interpreter.AllocateTensors();

  while (true) {
    TfLiteTensor* input = interpreter.input(0);

    GetImage(error_reporter, IN_IMG_WIDTH, IN_IMG_HEIGHT, NUM_OUT_CH,
             camera_buffer);

    ResizeConvertImage(error_reporter, IN_IMG_WIDTH, IN_IMG_HEIGHT, NUM_IN_CH,
                       CNN_IMG_SIZE, CNN_IMG_SIZE, NUM_OUT_CH, camera_buffer,
                       input->data.uint8);

    if (input->type != kTfLiteUInt8) {
      error_reporter->Report("Wrong input type.");
    }

    TfLiteStatus invoke_status = interpreter.Invoke();
    if (invoke_status != kTfLiteOk) {
      error_reporter->Report("Invoke failed.");
      break;
    }

    display_image_rgb565(IN_IMG_WIDTH, IN_IMG_HEIGHT, camera_buffer, 40, 40);
    display_image_rgb888(CNN_IMG_SIZE, CNN_IMG_SIZE, input->data.uint8, 300,
                         100);

    TfLiteTensor* output = interpreter.output(0);

    int top_ind = get_top_prediction(output->data.uint8, 10);
    print_prediction(labels[top_ind]);
    print_confidence(output->data.uint8[top_ind]);
  }

  return 0;
}
