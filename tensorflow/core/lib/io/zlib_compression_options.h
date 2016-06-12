/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_LIB_IO_ZLIB_COMPRESSION_OPTIONS_H_
#define TENSORFLOW_LIB_IO_ZLIB_COMPRESSION_OPTIONS_H_

#include <zlib.h>

namespace tensorflow {
namespace io {
class ZlibCompressionOptions {
 public:
  uint8 flush_mode = Z_SYNC_FLUSH;
};
}
}

#endif  // TENSORFLOW_LIB_IO_ZLIB_COMPRESSION_OPTIONS_H_
