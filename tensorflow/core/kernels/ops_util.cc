/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

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

#include <algorithm>
#include <cmath>

#include "tensorflow/core/kernels/ops_util.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/strings/str_util.h"
#include "tensorflow/core/util/padding.h"

namespace tensorflow {

Status GetWindowedOutputSizeVerbose(int64 input_size, int64 filter_size,
                                    int64 stride, Padding padding_type,
                                    int64* output_size, int64* padding_before,
                                    int64* padding_after) {
  switch (padding_type) {
    case Padding::VALID:
      *output_size = (input_size - filter_size + stride) / stride;
      *padding_before = *padding_after = 0;
      break;
    case Padding::SAME:
      *output_size = (input_size + stride - 1) / stride;
      const int64 padding_needed =
          std::max(0LL, (*output_size - 1) * stride + filter_size - input_size);
      // For odd values of total padding, add more padding at the 'right'
      // side of the given dimension.
      *padding_before = padding_needed / 2;
      *padding_after = padding_needed - *padding_before;
      break;
  }
  if (*output_size < 0) {
    return errors::InvalidArgument("computed output size would be negative");
  }
  return Status::OK();
}

Status GetWindowedOutputSize(int64 input_size, int64 filter_size, int64 stride,
                             Padding padding_type, int64* output_size,
                             int64* padding) {
  int64 padding_after_unused;
  return GetWindowedOutputSizeVerbose(input_size, filter_size, stride,
                                      padding_type, output_size, padding,
                                      &padding_after_unused);
}

Status Get3dOutputSize(const std::array<int64, 3>& input,
                       const std::array<int64, 3>& window,
                       const std::array<int64, 3>& strides,
                       Padding padding_type, std::array<int64, 3>* output_ptr,
                       std::array<int64, 3>* padding_ptr) {
  for (size_t i = 0; i < input.size(); ++i) {
    TF_RETURN_IF_ERROR(GetWindowedOutputSize(input[i], window[i], strides[i],
                                             padding_type, &(*output_ptr)[i],
                                             &(*padding_ptr)[i]));
  }
  return Status::OK();
}

Eigen::PaddingType BrainPadding2EigenPadding(Padding padding) {
  switch (padding) {
    case Padding::VALID:
      return Eigen::PADDING_VALID;
    case Padding::SAME:
      return Eigen::PADDING_SAME;
  }
  return Eigen::PADDING_SAME;  // Prevent compiler warning about missing return
}

Status GetBroadcastSize(const int index, const int in_size, const int ksize,
                        const int stride, const int pad_size, int* bindex,
                        int* bsize) {
  // Cannot have strides larger than the patch size.
  if (stride > ksize) {
    return errors::InvalidArgument(
        "stride must be less than or equal to kernel size");
  }
  // Cannot have index beyond the input size.
  if (index * stride > in_size) {
    return errors::InvalidArgument(
        "index * stride must be less than or equal to input size");
  }
  *bindex = index * stride;
  *bsize = ksize;
  if (*bindex < pad_size) {
    // If the current index is in the padding area, start broadcast  from index
    // 0 with broadcast size reduced by padding size.
    *bsize = ksize + *bindex - pad_size;
    *bindex = 0;
  } else {
    // Otherwise, start broadcast from current index reduced by padding size.
    *bindex -= pad_size;
  }
  if (*bindex + ksize > in_size) {
    *bsize = std::min((in_size - *bindex), ksize);
  }
  return Status::OK();
}

string SanitizeThreadSuffix(string suffix) {
  string clean;
  for (int i = 0; i < suffix.size(); ++i) {
    const char ch = suffix[i];
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') || ch == '_' || ch == '-') {
      clean += ch;
    } else {
      clean += '_';
    }
  }
  return clean;
}

}  // namespace tensorflow
