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

#include "tensorflow/lite/experimental/micro/examples/micro_speech/micro_features/no_micro_features_data.h"

/* File automatically created by
 * tensorflow/examples/speech_commands/wav_to_features.py \
 * --sample_rate=16000 \
 * --clip_duration_ms=1000 \
 * --window_size_ms=30 \
 * --window_stride_ms=20 \
 * --feature_bin_count=40 \
 * --quantize=1 \
 * --preprocess="micro" \
 * --input_wav="speech_commands_test_set_v0.02/no/f9643d42_nohash_4.wav" \
 * --output_c_file="/tmp/no_micro_features_data.cc" \
 */

const int g_no_micro_f9643d42_nohash_4_width = 40;
const int g_no_micro_f9643d42_nohash_4_height = 49;
const unsigned char g_no_micro_f9643d42_nohash_4_data[] = {
    230, 205, 191, 203, 202, 181, 180, 194, 205, 187, 183, 197, 203, 198, 196,
    186, 202, 159, 151, 126, 110, 138, 141, 142, 137, 148, 133, 120, 110, 126,
    117, 110, 117, 116, 137, 134, 95,  116, 123, 110, 184, 144, 183, 189, 197,
    172, 188, 164, 194, 179, 175, 174, 182, 173, 184, 174, 200, 145, 154, 148,
    147, 135, 143, 122, 127, 138, 116, 99,  122, 105, 110, 125, 127, 133, 131,
    123, 116, 119, 127, 114, 193, 176, 185, 170, 175, 146, 166, 167, 185, 185,
    185, 183, 195, 185, 176, 178, 197, 155, 137, 144, 164, 132, 153, 132, 138,
    137, 134, 95,  120, 116, 131, 122, 99,  120, 120, 110, 116, 110, 126, 127,
    128, 159, 187, 119, 178, 187, 197, 167, 199, 184, 180, 165, 194, 176, 144,
    134, 187, 136, 142, 134, 145, 132, 145, 105, 119, 123, 125, 116, 125, 102,
    129, 138, 130, 99,  99,  90,  120, 123, 134, 95,  194, 172, 187, 123, 191,
    179, 195, 182, 201, 137, 167, 142, 185, 161, 187, 146, 167, 152, 154, 107,
    152, 112, 134, 144, 117, 116, 105, 85,  105, 105, 99,  90,  123, 112, 112,
    68,  107, 105, 117, 99,  116, 143, 139, 90,  154, 142, 188, 172, 178, 135,
    175, 149, 177, 110, 173, 160, 169, 162, 173, 119, 132, 110, 85,  85,  117,
    129, 117, 112, 117, 51,  112, 95,  139, 102, 105, 90,  128, 119, 112, 99,
    170, 168, 195, 152, 174, 173, 180, 0,   157, 130, 169, 149, 149, 123, 170,
    130, 170, 133, 159, 102, 134, 90,  85,  105, 126, 119, 130, 90,  78,  68,
    127, 120, 95,  51,  122, 110, 112, 78,  116, 95,  180, 135, 179, 146, 179,
    162, 197, 153, 172, 135, 154, 0,   149, 95,  145, 114, 166, 0,   114, 110,
    145, 107, 114, 90,  136, 68,  95,  95,  95,  85,  116, 99,  116, 0,   95,
    68,  102, 51,  102, 78,  185, 157, 138, 158, 180, 117, 173, 142, 145, 117,
    169, 130, 159, 99,  138, 123, 169, 90,  78,  0,   123, 85,  107, 51,  114,
    102, 95,  0,   116, 85,  119, 95,  95,  68,  85,  51,  116, 68,  102, 78,
    167, 105, 164, 163, 178, 126, 164, 154, 154, 51,  177, 120, 156, 85,  134,
    139, 168, 90,  161, 102, 114, 116, 122, 95,  112, 102, 107, 51,  114, 85,
    119, 78,  114, 90,  102, 51,  102, 51,  114, 99,  177, 68,  152, 102, 184,
    166, 179, 129, 177, 129, 180, 110, 158, 105, 139, 0,   145, 85,  148, 102,
    117, 102, 116, 0,   78,  68,  90,  51,  107, 85,  78,  0,   51,  0,   51,
    0,   95,  51,  107, 68,  180, 117, 90,  0,   138, 0,   187, 146, 119, 140,
    164, 90,  136, 0,   131, 51,  159, 99,  141, 138, 116, 51,  90,  51,  90,
    68,  105, 0,   85,  78,  112, 51,  122, 95,  128, 68,  85,  0,   112, 68,
    147, 126, 178, 146, 171, 130, 190, 147, 188, 123, 170, 78,  132, 0,   130,
    125, 159, 95,  102, 0,   110, 0,   95,  85,  120, 68,  78,  51,  99,  51,
    105, 0,   112, 102, 105, 68,  90,  51,  90,  0,   127, 95,  166, 175, 187,
    133, 135, 0,   171, 139, 132, 128, 140, 51,  126, 107, 161, 0,   95,  51,
    119, 0,   114, 0,   95,  110, 116, 51,  112, 0,   90,  0,   116, 51,  68,
    0,   105, 68,  105, 0,   164, 78,  173, 0,   194, 166, 145, 114, 116, 51,
    107, 122, 151, 0,   156, 102, 148, 51,  122, 95,  129, 0,   85,  0,   127,
    78,  90,  0,   78,  0,   95,  0,   110, 0,   68,  119, 120, 68,  68,  0,
    122, 99,  147, 127, 200, 167, 85,  114, 161, 85,  161, 125, 143, 99,  156,
    85,  147, 68,  99,  0,   107, 102, 132, 51,  112, 68,  95,  78,  99,  0,
    68,  0,   51,  0,   90,  78,  128, 51,  95,  0,   166, 136, 174, 138, 189,
    144, 130, 129, 138, 134, 132, 120, 134, 0,   51,  78,  147, 51,  51,  0,
    51,  0,   78,  0,   68,  68,  95,  78,  90,  0,   0,   0,   68,  0,   90,
    68,  110, 0,   95,  51,  165, 151, 157, 0,   0,   0,   112, 0,   112, 95,
    149, 107, 119, 68,  126, 68,  138, 0,   78,  0,   78,  0,   99,  51,  112,
    0,   102, 0,   78,  51,  85,  0,   0,   0,   78,  0,   95,  0,   95,  78,
    105, 0,   152, 0,   0,   51,  132, 105, 159, 0,   129, 102, 114, 0,   138,
    51,  123, 0,   129, 78,  119, 51,  51,  51,  105, 0,   78,  85,  95,  0,
    85,  0,   0,   0,   85,  0,   78,  0,   0,   0,   172, 142, 141, 0,   137,
    0,   148, 128, 157, 120, 146, 120, 120, 0,   95,  78,  141, 68,  68,  0,
    68,  0,   90,  0,   85,  0,   107, 0,   78,  0,   85,  51,  102, 0,   68,
    78,  68,  0,   51,  0,   125, 0,   141, 51,  102, 138, 175, 51,  120, 51,
    173, 85,  116, 141, 164, 68,  150, 123, 133, 51,  114, 0,   117, 68,  150,
    51,  116, 68,  78,  0,   68,  0,   68,  0,   85,  0,   78,  0,   51,  78,
    155, 90,  161, 0,   132, 99,  123, 78,  107, 0,   134, 90,  95,  0,   78,
    0,   162, 143, 85,  0,   107, 78,  125, 90,  90,  51,  51,  0,   85,  0,
    0,   0,   132, 102, 102, 154, 128, 0,   99,  68,  162, 102, 151, 0,   99,
    51,  147, 141, 156, 0,   112, 120, 158, 127, 145, 139, 187, 171, 135, 138,
    146, 0,   95,  68,  127, 0,   85,  0,   105, 0,   0,   0,   187, 170, 162,
    188, 165, 51,  51,  78,  243, 215, 225, 196, 205, 181, 205, 168, 176, 134,
    157, 110, 126, 114, 133, 139, 193, 163, 159, 116, 160, 126, 122, 127, 171,
    99,  114, 68,  123, 85,  90,  0,   157, 146, 166, 179, 136, 0,   116, 90,
    242, 219, 240, 204, 216, 164, 188, 171, 176, 164, 154, 158, 190, 157, 190,
    141, 182, 177, 169, 128, 172, 145, 105, 129, 157, 90,  78,  51,  119, 68,
    137, 68,  116, 78,  141, 132, 151, 122, 156, 140, 234, 206, 229, 201, 216,
    174, 191, 144, 162, 85,  122, 157, 194, 167, 204, 149, 180, 166, 166, 139,
    122, 133, 156, 126, 145, 85,  128, 0,   99,  51,  145, 0,   126, 51,  166,
    162, 166, 162, 177, 157, 228, 198, 221, 197, 214, 177, 173, 166, 173, 139,
    185, 191, 202, 163, 205, 172, 206, 189, 135, 68,  166, 134, 149, 134, 135,
    90,  127, 107, 175, 90,  136, 117, 135, 140, 172, 167, 166, 149, 177, 152,
    221, 191, 215, 194, 211, 0,   156, 147, 182, 178, 208, 163, 190, 157, 208,
    200, 195, 164, 179, 154, 181, 150, 143, 99,  132, 137, 185, 143, 163, 85,
    51,  107, 132, 134, 164, 127, 167, 159, 175, 141, 216, 195, 223, 211, 238,
    223, 243, 215, 226, 204, 232, 211, 232, 213, 240, 218, 235, 214, 238, 205,
    207, 173, 149, 201, 215, 200, 230, 213, 208, 195, 175, 151, 195, 175, 182,
    163, 235, 217, 218, 190, 211, 191, 215, 191, 217, 220, 241, 215, 229, 206,
    236, 210, 227, 216, 236, 188, 183, 149, 202, 189, 208, 172, 191, 201, 220,
    193, 221, 207, 216, 208, 201, 131, 170, 187, 229, 197, 211, 194, 226, 201,
    205, 184, 206, 177, 221, 210, 226, 184, 204, 197, 218, 198, 212, 209, 213,
    141, 172, 110, 175, 167, 180, 156, 213, 188, 192, 179, 213, 205, 204, 174,
    200, 147, 162, 181, 203, 167, 198, 187, 210, 164, 196, 169, 189, 168, 224,
    198, 213, 204, 198, 195, 230, 211, 221, 197, 208, 0,   0,   0,   85,  90,
    167, 130, 175, 173, 203, 164, 193, 144, 170, 145, 185, 148, 154, 139, 198,
    159, 180, 171, 216, 174, 178, 161, 166, 136, 216, 184, 215, 197, 199, 190,
    228, 195, 208, 51,  117, 0,   0,   0,   0,   0,   140, 51,  135, 154, 188,
    155, 168, 0,   90,  0,   156, 85,  110, 0,   174, 90,  172, 154, 179, 99,
    142, 166, 179, 157, 177, 95,  192, 142, 204, 198, 217, 147, 173, 0,   112,
    0,   0,   0,   0,   0,   0,   0,   110, 0,   107, 0,   160, 0,   148, 95,
    172, 0,   0,   0,   116, 0,   122, 114, 170, 0,   0,   0,   0,   0,   179,
    110, 196, 85,  205, 183, 169, 0,   99,  0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   141, 0,   112, 0,   0,   0,   134, 0,   0,   0,   0,
    0,   0,   0,   139, 0,   0,   0,   0,   112, 186, 78,  163, 0,   169, 128,
    174, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   95,
    0,   105, 0,   0,   0,   105, 0,   0,   0,   0,   0,   0,   0,   95,  0,
    0,   0,   0,   0,   0,   0,   119, 0,   164, 78,  0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   90,  0,   0,   68,
    117, 0,   0,   0,   0,   0,   0,   0,   148, 0,   0,   0,   0,   0,   0,
    0,   0,   0,   116, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   51,
    0,   0,   0,   99,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   99,  0,   0,   0,   0,   0,   0,   0,   0,   0,   78,  0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};
