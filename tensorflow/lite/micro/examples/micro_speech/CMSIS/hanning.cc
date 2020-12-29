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

#include "tensorflow/lite/micro/examples/micro_speech/CMSIS/hanning.h"

const int g_hanning_size = 480;
const int16_t g_hanning[480] = {
    0,     1,     5,     12,    22,    35,    50,    69,    90,    114,   140,
    170,   202,   237,   275,   316,   359,   405,   454,   506,   560,   617,
    677,   740,   805,   873,   943,   1016,  1092,  1171,  1252,  1336,  1422,
    1511,  1602,  1696,  1793,  1892,  1993,  2097,  2204,  2312,  2424,  2537,
    2653,  2772,  2893,  3016,  3141,  3269,  3399,  3531,  3665,  3802,  3941,
    4082,  4225,  4370,  4517,  4666,  4817,  4971,  5126,  5283,  5442,  5603,
    5765,  5930,  6096,  6265,  6435,  6606,  6779,  6954,  7131,  7309,  7489,
    7670,  7853,  8037,  8223,  8410,  8598,  8788,  8979,  9171,  9365,  9560,
    9756,  9953,  10151, 10350, 10551, 10752, 10954, 11157, 11362, 11567, 11772,
    11979, 12186, 12395, 12603, 12813, 13023, 13233, 13445, 13656, 13868, 14081,
    14294, 14507, 14721, 14935, 15149, 15363, 15578, 15793, 16008, 16222, 16437,
    16652, 16867, 17082, 17297, 17511, 17725, 17939, 18153, 18367, 18580, 18793,
    19005, 19217, 19428, 19639, 19850, 20059, 20269, 20477, 20685, 20892, 21098,
    21303, 21508, 21712, 21914, 22116, 22317, 22517, 22716, 22913, 23110, 23305,
    23499, 23692, 23884, 24075, 24264, 24451, 24638, 24823, 25006, 25188, 25369,
    25548, 25725, 25901, 26075, 26247, 26418, 26587, 26754, 26920, 27083, 27245,
    27405, 27563, 27719, 27874, 28026, 28176, 28324, 28470, 28614, 28756, 28896,
    29034, 29169, 29303, 29434, 29563, 29689, 29813, 29935, 30055, 30172, 30287,
    30400, 30510, 30617, 30723, 30825, 30926, 31023, 31119, 31211, 31301, 31389,
    31474, 31556, 31636, 31713, 31788, 31860, 31929, 31996, 32059, 32121, 32179,
    32235, 32288, 32338, 32386, 32430, 32472, 32512, 32548, 32582, 32613, 32641,
    32666, 32689, 32708, 32725, 32739, 32751, 32759, 32765, 32767, 32767, 32765,
    32759, 32751, 32739, 32725, 32708, 32689, 32666, 32641, 32613, 32582, 32548,
    32512, 32472, 32430, 32386, 32338, 32288, 32235, 32179, 32121, 32059, 31996,
    31929, 31860, 31788, 31713, 31636, 31556, 31474, 31389, 31301, 31211, 31119,
    31023, 30926, 30825, 30723, 30617, 30510, 30400, 30287, 30172, 30055, 29935,
    29813, 29689, 29563, 29434, 29303, 29169, 29034, 28896, 28756, 28614, 28470,
    28324, 28176, 28026, 27874, 27719, 27563, 27405, 27245, 27083, 26920, 26754,
    26587, 26418, 26247, 26075, 25901, 25725, 25548, 25369, 25188, 25006, 24823,
    24638, 24451, 24264, 24075, 23884, 23692, 23499, 23305, 23110, 22913, 22716,
    22517, 22317, 22116, 21914, 21712, 21508, 21303, 21098, 20892, 20685, 20477,
    20269, 20059, 19850, 19639, 19428, 19217, 19005, 18793, 18580, 18367, 18153,
    17939, 17725, 17511, 17297, 17082, 16867, 16652, 16437, 16222, 16008, 15793,
    15578, 15363, 15149, 14935, 14721, 14507, 14294, 14081, 13868, 13656, 13445,
    13233, 13023, 12813, 12603, 12395, 12186, 11979, 11772, 11567, 11362, 11157,
    10954, 10752, 10551, 10350, 10151, 9953,  9756,  9560,  9365,  9171,  8979,
    8788,  8598,  8410,  8223,  8037,  7853,  7670,  7489,  7309,  7131,  6954,
    6779,  6606,  6435,  6265,  6096,  5930,  5765,  5603,  5442,  5283,  5126,
    4971,  4817,  4666,  4517,  4370,  4225,  4082,  3941,  3802,  3665,  3531,
    3399,  3269,  3141,  3016,  2893,  2772,  2653,  2537,  2424,  2312,  2204,
    2097,  1993,  1892,  1793,  1696,  1602,  1511,  1422,  1336,  1252,  1171,
    1092,  1016,  943,   873,   805,   740,   677,   617,   560,   506,   454,
    405,   359,   316,   275,   237,   202,   170,   140,   114,   90,    69,
    50,    35,    22,    12,    5,     1,     0};
