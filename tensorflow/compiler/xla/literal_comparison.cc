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

#include "tensorflow/compiler/xla/literal_comparison.h"

#include <unistd.h>
#include <cmath>
#include <vector>

#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/core/lib/core/casts.h"
#include "tensorflow/core/lib/strings/strcat.h"
#include "tensorflow/core/platform/env.h"

using tensorflow::strings::Appendf;
using tensorflow::strings::Printf;
using tensorflow::strings::StrAppend;
using tensorflow::strings::StrCat;

namespace xla {
namespace literal_comparison {
namespace {

// Helper function for comparing a floating point type, FloatT, bitwise equal
// between the left-hand-side and right-hand-side, by bit-casting to UnsignedT
// -- on miscompare, a nice error message is given in the AssertionFailure.
template <typename FloatT, typename UnsignedT>
Status CompareFloatsBitwiseEqual(FloatT lhs, FloatT rhs) {
  auto ulhs = tensorflow::bit_cast<UnsignedT>(lhs);
  auto urhs = tensorflow::bit_cast<UnsignedT>(rhs);
  auto lhs_double = static_cast<double>(lhs);
  auto rhs_double = static_cast<double>(rhs);
  if (ulhs != urhs) {
    return InvalidArgument(
        "floating values are not bitwise-equal; and equality testing "
        "was requested: %s=%g=%a vs %s=%g=%a",
        StrCat(tensorflow::strings::Hex(ulhs)).c_str(), lhs_double, lhs_double,
        StrCat(tensorflow::strings::Hex(urhs)).c_str(), rhs_double, rhs_double);
  }
  return Status::OK();
}

// Templated comparator that specializes for float equality comparison with the
// bitwise helper above (this is the un-specialized fallback, to just use the
// default gunit implementation).
template <typename NativeT>
Status CompareEqual(NativeT lhs, NativeT rhs) {
  if (lhs == rhs) {
    return Status::OK();
  }
  return InvalidArgument("Expected equality of these values:\n  %s\n  %s",
                         StrCat(lhs).c_str(), StrCat(rhs).c_str());
}

// Specializations for floating types that do bitwise comparisons when equality
// comparison is requested.
template <>
Status CompareEqual<bfloat16>(bfloat16 lhs, bfloat16 rhs) {
  return CompareFloatsBitwiseEqual<bfloat16, uint16>(lhs, rhs);
}
template <>
Status CompareEqual<Eigen::half>(Eigen::half lhs, Eigen::half rhs) {
  return CompareFloatsBitwiseEqual<Eigen::half, uint16>(lhs, rhs);
}
template <>
Status CompareEqual<float>(float lhs, float rhs) {
  return CompareFloatsBitwiseEqual<float, uint32>(lhs, rhs);
}
template <>
Status CompareEqual<double>(double lhs, double rhs) {
  return CompareFloatsBitwiseEqual<double, uint64>(lhs, rhs);
}
template <>
Status CompareEqual<complex64>(complex64 lhs, complex64 rhs) {
  auto res = CompareEqual<float>(lhs.real(), rhs.real());
  if (!res.ok()) {
    return res;
  }
  return CompareEqual<float>(lhs.imag(), rhs.imag());
}

// A recursive function which iterates through every index of expected and
// actual literal and compares their values elementwise. Returns true if all
// elements are equal.
template <typename NativeT>
Status Equal(LiteralSlice expected, LiteralSlice actual,
             tensorflow::gtl::MutableArraySlice<int64> multi_index,
             int64 dimension) {
  if (dimension == expected.shape().dimensions_size()) {
    NativeT expected_value = expected.Get<NativeT>(multi_index);
    NativeT actual_value = actual.Get<NativeT>(multi_index);
    return CompareEqual<NativeT>(expected_value, actual_value);
  }

  Status result;
  for (int64 i = 0; i < expected.shape().dimensions(dimension); ++i) {
    multi_index[dimension] = i;
    result.Update(Equal<NativeT>(expected, actual, multi_index, dimension + 1));
  }
  return result;
}

// Gets the total element count.  For tuples, this is not the count of tuple
// elements, but the sum of elements of each tuple element.
int64 RecursiveElementCount(const Shape& shape) {
  if (ShapeUtil::IsTuple(shape)) {
    const int64 tuple_elements = ShapeUtil::TupleElementCount(shape);
    int64 total = 0;
    for (int64 i = 0; i < tuple_elements; ++i) {
      total += RecursiveElementCount(ShapeUtil::GetTupleElementShape(shape, i));
    }
    return total;
  } else {
    return ShapeUtil::ElementsIn(shape);
  }
}

// Returns whether the actual and expected values are mismatched with respect to
// nans. 'relaxed_nans' is interpreted as in xla::ErrorSpec.
template <typename NativeT>
bool NanMismatch(NativeT expected, NativeT actual, bool relaxed_nans) {
  if (relaxed_nans) {
    return !std::isnan(expected) && std::isnan(actual);
  } else {
    return std::isnan(expected) != std::isnan(actual);
  }
}

template <>
bool NanMismatch<complex64>(complex64 expected, complex64 actual,
                            bool relaxed_nans) {
  return NanMismatch<float>(expected.real(), actual.real(), relaxed_nans) ||
         NanMismatch<float>(expected.imag(), actual.imag(), relaxed_nans);
}

template <>
bool NanMismatch<half>(half expected, half actual, bool relaxed_nans) {
  return NanMismatch<float>(static_cast<float>(expected),
                            static_cast<float>(actual), relaxed_nans);
}

// Converts the given floating-point value to a string.
template <typename NativeT>
string FpValueToString(NativeT value) {
  return Printf("%8.4g", static_cast<double>(value));
}

template <>
string FpValueToString<complex64>(complex64 value) {
  return Printf("%8.4g + %8.4fi", value.real(), value.imag());
}

// Returns the absolute value of the given floating point value. This function
// is used instead of std::abs directly in order to allow type-dependent
// implementations for NearComparator.
template <typename NativeT>
float FpAbsoluteValue(NativeT value) {
  return std::abs(value);
}

template <>
float FpAbsoluteValue(bfloat16 value) {
  return FpAbsoluteValue<float>(static_cast<float>(value));
}

template <>
float FpAbsoluteValue(half value) {
  return FpAbsoluteValue<float>(static_cast<float>(value));
}

// Helper class for comparing floating-point literals within an error bound.
template <typename NativeT>
class NearComparator {
 public:
  // Compares the two array literals elementwise and returns a comparison
  // result. The comparison is ok() if all actual and expected elements are
  // within the given error bound. In case of error, the status contains a
  // detailed message about the discrepancy.
  static Status Compare(const LiteralSlice& expected,
                        const LiteralSlice& actual, ErrorSpec error,
                        bool detailed_message,
                        const MiscompareCallback& miscompare_callback) {
    NearComparator<NativeT> comparator(expected, actual, error,
                                       detailed_message, miscompare_callback);
    return comparator.Run();
  }

 private:
  // Data structure encapsulating metadata about a single element mismatch.
  struct Mismatch {
    NativeT actual;
    NativeT expected;
    float rel_error;
    float abs_error;

    // The linear index of the failure within the shape. This linear index is
    // from the 'actual' literal.
    int64 linear_index;

    bool operator<(const Mismatch& other) const {
      return rel_error < other.rel_error;
    }

    string ToString(const Shape& shape) const {
      return Printf(
          "actual %s, expected %s, index %s, rel error %8.3g, abs error %8.3g",
          FpValueToString(actual).c_str(), FpValueToString(expected).c_str(),
          Literal::MultiIndexAsString(
              IndexUtil::LinearIndexToMultidimensionalIndex(shape,
                                                            linear_index))
              .c_str(),
          rel_error, abs_error);
    }
  };

  NearComparator(const LiteralSlice& expected, const LiteralSlice& actual,
                 ErrorSpec error, bool detailed_message,
                 const MiscompareCallback& miscompare_callback)
      : expected_(expected),
        actual_(actual),
        error_(error),
        detailed_message_(detailed_message),
        miscompare_callback_(miscompare_callback),
        abs_value_buckets_(kAbsValueBucketBounds.size() - 1, {0, 0}),
        abs_error_buckets_(kErrorBucketBounds.size(), 0),
        rel_error_buckets_(kErrorBucketBounds.size(), 0) {}

  // Runs the comparison between expected and actual literals.
  Status Run() {
    VLOG(1) << "expected:";
    XLA_VLOG_LINES(1, ToStringTruncated(expected_));
    VLOG(1) << "actual:";
    XLA_VLOG_LINES(1, ToStringTruncated(actual_));

    // If the shapes mismatch, we simply fail the expectation instead of
    // printing out data, as it's a type error rather than a value error.
    TF_RETURN_IF_ERROR(EqualShapes(expected_.shape(), actual_.shape()));
    if (!ShapeUtil::IsArray(expected_.shape())) {
      return InvalidArgument("Expected array shape; got %s.",
                             ShapeUtil::HumanString(expected_.shape()).c_str());
    }

    mismatches_ = Literal(ShapeUtil::ChangeElementType(actual_.shape(), PRED));
    mismatches_.PopulateWithValue(false);

    CompareLiterals();

    if (num_mismatches_ == 0) {
      return Status::OK();
    } else if (!VLOG_IS_ON(1) && miscompare_callback_ != nullptr) {
      miscompare_callback_(expected_, actual_, mismatches_);
    }
    return InvalidArgument("%s", ErrorMessage().c_str());
  }

  // Insert the given absolute value into the absolute value bucket vector. The
  // bounds of the buckets are given by kAbsValueBucketBounds.
  void UpdateAbsValueBucket(NativeT value, bool is_mismatch) {
    // Adjust the bucket containing the absolute values of the 'actual'
    // elements.
    const float abs_value = FpAbsoluteValue(value);
    for (int i = 0; i < abs_value_buckets_.size(); ++i) {
      if (i == abs_value_buckets_.size() - 1 ||
          (abs_value >= kAbsValueBucketBounds[i] &&
           abs_value < kAbsValueBucketBounds[i + 1])) {
        // The first value of the pair is the count of elements in the bucket,
        // the second is the count of mismatches in the bucket.
        abs_value_buckets_[i].first++;
        if (is_mismatch) {
          abs_value_buckets_[i].second++;
        }
        return;
      }
    }
  }

  // Insert the given error into the given error bucket vector.
  void UpdateErrorBucket(
      float error, tensorflow::gtl::MutableArraySlice<int64> error_buckets) {
    CHECK_EQ(error_buckets.size(), kErrorBucketBounds.size());
    for (int i = 0; i < error_buckets.size(); ++i) {
      if (error >= kErrorBucketBounds[i]) {
        error_buckets[i]++;
      }
    }
  }

  // Compares the two given elements from the expected and actual literals at
  // the given literal_index and keeps track of various mismatch statistics.
  void CompareValues(NativeT expected, NativeT actual, int64 linear_index) {
    const bool is_nan_mismatch =
        NanMismatch(expected, actual, error_.relaxed_nans);
    float abs_error;
    float rel_error;
    if (actual == expected) {
      abs_error = 0;
      rel_error = 0;
    } else if (is_nan_mismatch) {
      num_nan_mismatches_++;
      // A nan mismatch is considered to have infinite error. rel_error is used
      // for sorting a std::set of the top mismatchs, and a nan value here will
      // result in undefined behavior because nan's do not satisfy the strict
      // weak ordering requirement of std containers.
      abs_error = std::numeric_limits<float>::infinity();
      rel_error = std::numeric_limits<float>::infinity();
    } else {
      abs_error = FpAbsoluteValue(actual - expected);
      rel_error = abs_error / FpAbsoluteValue(expected);
    }
    const bool is_abs_mismatch = abs_error > error_.abs;
    const bool is_rel_mismatch = rel_error > error_.rel;
    const bool is_mismatch =
        is_nan_mismatch || (is_abs_mismatch && is_rel_mismatch);

    // Update the error of the relative bucket only if the *absolute* error
    // bound is exceeded and vice versa.
    if (is_abs_mismatch) {
      num_abs_mismatches_++;
      UpdateErrorBucket(rel_error, &rel_error_buckets_);
    }
    if (is_rel_mismatch) {
      num_rel_mismatches_++;
      UpdateErrorBucket(abs_error, &abs_error_buckets_);
    }

    UpdateAbsValueBucket(actual, is_mismatch);

    if (!is_mismatch) {
      return;
    }

    num_mismatches_++;

    // Keep track of the kTopRelativeErrorCount relative error mismatches.
    if (top_rel_mismatches_.size() < kTopRelativeErrorCount ||
        rel_error > top_rel_mismatches_.begin()->rel_error) {
      Mismatch mismatch = {actual, expected, rel_error, abs_error,
                           linear_index};
      top_rel_mismatches_.insert(mismatch);
      if (top_rel_mismatches_.size() > kTopRelativeErrorCount) {
        top_rel_mismatches_.erase(top_rel_mismatches_.begin());
      }
    }

    mismatches_.data<bool>()[linear_index] = true;
  }

  // Compares the two literals elementwise.
  void CompareLiterals() {
    // Fast path optimization for the case were layouts match.
    if (LayoutUtil::Equal(actual_.shape().layout(),
                          expected_.shape().layout())) {
      tensorflow::gtl::ArraySlice<const NativeT> expected_data =
          expected_.data<NativeT>();
      tensorflow::gtl::ArraySlice<const NativeT> actual_data =
          actual_.data<NativeT>();
      const int64 len = expected_data.size();
      for (int64 i = 0; i < len; ++i) {
        CompareValues(expected_data[i], actual_data[i], i);
      }
      return;
    }
    std::vector<int64> multi_index(ShapeUtil::Rank(actual_.shape()), 0);
    CompareLiteralsSlow(0, &multi_index);
  }

  // Slow path for CompareLiterals when 'actual' and 'expected' literals have
  // different layouts. In this case, multidimensional indices are constructed
  // and indexed for each element.
  void CompareLiteralsSlow(int64 dimension, std::vector<int64>* multi_index) {
    if (dimension == multi_index->size()) {
      CompareValues(expected_.Get<NativeT>(*multi_index),
                    actual_.Get<NativeT>(*multi_index),
                    IndexUtil::MultidimensionalIndexToLinearIndex(
                        actual_.shape(), *multi_index));
    } else {
      for (int64 i = 0; i < expected_.shape().dimensions(dimension); ++i) {
        (*multi_index)[dimension] = i;
        CompareLiteralsSlow(dimension + 1, multi_index);
      }
    }
  }

  // Returns an error message string with a detailed breakdown of the
  // mismatches. Called after calling Run().
  string ErrorMessage() {
    string out;
    int64 element_count = ShapeUtil::ElementsIn(actual_.shape());

    auto percent_string = [](float a, float b) {
      float pct = b == 0.0 ? 0.0 : 100.0 * a / b;
      return Printf("%0.4f%%", pct);
    };

    Appendf(&out,
            "\nMismatch count %lld (%s) in shape %s (%lld elements), abs bound "
            "%g, rel bound %g\n",
            num_mismatches_,
            percent_string(num_mismatches_, element_count).c_str(),
            ShapeUtil::HumanString(actual_.shape()).c_str(),
            ShapeUtil::ElementsIn(actual_.shape()), error_.abs, error_.rel);
    if (num_nan_mismatches_ > 0) {
      StrAppend(&out, "nan mismatches ", num_nan_mismatches_, "\n");
    }
    Appendf(&out, "Top relative error mismatches:\n");
    for (auto it = top_rel_mismatches_.rbegin();
         it != top_rel_mismatches_.rend(); ++it) {
      StrAppend(&out, "  ", it->ToString(actual_.shape()).c_str(), "\n");
    }

    if (!detailed_message_) {
      return out;
    }

    StrAppend(&out, "Absolute magnitude breakdown of actual values:\n");
    CHECK_EQ(abs_value_buckets_.size() + 1, kAbsValueBucketBounds.size());
    for (int i = 0; i < abs_value_buckets_.size(); ++i) {
      const int64 bucket_size = abs_value_buckets_[i].first;
      const int64 bucket_mismatches = abs_value_buckets_[i].second;
      string mismatch_str = bucket_mismatches > 0
                                ? Printf(", mismatches %lld", bucket_mismatches)
                                : "";
      Appendf(&out, "  %-6g <= x < %-6g : %7lld (%9s)%s\n",
              kAbsValueBucketBounds[i], kAbsValueBucketBounds[i + 1],
              bucket_size, percent_string(bucket_size, element_count).c_str(),
              mismatch_str.c_str());
    }

    auto print_accum_buckets = [&](const string& header, int64 total,
                                   tensorflow::gtl::ArraySlice<int64> buckets) {
      StrAppend(&out, header, ":\n");
      Appendf(&out, "  <  %-6g : %7lld (%s)\n", kErrorBucketBounds[0],
              total - buckets[0],
              percent_string(total - buckets[0], total).c_str());
      CHECK_EQ(buckets.size(), kErrorBucketBounds.size());
      for (int i = 0; i < kErrorBucketBounds.size(); ++i) {
        Appendf(&out, "  >= %-6g : %7lld (%s)\n", kErrorBucketBounds[i],
                buckets[i], percent_string(buckets[i], total).c_str());
      }
    };
    Appendf(&out, "Elements exceeding abs error bound %g: %lld (%s)\n",
            error_.abs, num_abs_mismatches_,
            percent_string(num_abs_mismatches_, element_count).c_str());
    print_accum_buckets(
        "Relative error breakdown of elements exceeding abs error bound",
        num_abs_mismatches_, rel_error_buckets_);
    Appendf(&out, "Elements exceeding rel error bound %g: %lld (%s)\n",
            error_.rel, num_rel_mismatches_,
            percent_string(num_rel_mismatches_, element_count).c_str());
    print_accum_buckets(
        "Absolute error breakdown of elements exceeding rel error bound",
        num_rel_mismatches_, abs_error_buckets_);
    return out;
  }

  // 'actual' and 'expected' literals being compared.
  LiteralSlice expected_;
  LiteralSlice actual_;

  // The error bounds of the comparison.
  ErrorSpec error_;

  // Whether to include detailed breakdown of mismatches in the error message.
  bool detailed_message_;

  // Callback to invoke on miscompare.
  MiscompareCallback miscompare_callback_;

  // Number of element element mismatches encountered so far.
  int64 num_mismatches_ = 0;

  // Number of elements with a nan mismatch.
  int64 num_nan_mismatches_ = 0;

  // Number of elements which exceed the absolute/relative error bound.
  int64 num_abs_mismatches_ = 0;
  int64 num_rel_mismatches_ = 0;

  // A Literal containing which elements did not match in the expected and
  // actual literals. mismatches_ contains PREDs and is of the same sizes as
  // the comparison literals.
  Literal mismatches_;

  // The number of mismatches to report in the output, sorted by relative error
  // magnitude.
  static constexpr int64 kTopRelativeErrorCount = 5;

  // The set of mismatches with the largest relative error. The size of this set
  // is bounded by kTopRelativeErrorCount.
  std::multiset<Mismatch> top_rel_mismatches_;

  // Actual values are bucketed by absolute value. kAbsValueBucketBounds is the
  // bounds of these buckets. abs_value_buckets_ contains a pair for each
  // bucket: the element count and failure count.
  static constexpr std::array<float, 7> kAbsValueBucketBounds = {
      0.0, 0.0001, 0.001, 0.01, 0.1, 1, std::numeric_limits<float>::infinity()};
  std::vector<std::pair<int64, int64>> abs_value_buckets_;

  // Buckets for relative and absolute errors. The relative error buckets only
  // contains those elements which exceed the *absolute* error bound, and vice
  // versa. This makes it easy to see the effect of adjusting the relative (or
  // absolute) error bound on the success of the comparison. kErrorBucketBounds
  // are the lower bounds of the buckets in both vectors. The error buckets are
  // a cumulative distribution so an error value may appear in more than one
  // bucket. For example an error value of 0.003 may appear in the buckets
  // bounded by 0.01, 0.1, and 1.0.
  static constexpr std::array<float, 5> kErrorBucketBounds = {0.0001, 0.001,
                                                              0.01, 0.1, 1};
  std::vector<int64> abs_error_buckets_;
  std::vector<int64> rel_error_buckets_;
};

template <typename NativeT>
constexpr std::array<float, 7> NearComparator<NativeT>::kAbsValueBucketBounds;
template <typename NativeT>
constexpr std::array<float, 5> NearComparator<NativeT>::kErrorBucketBounds;

// Helper function for comparing two literals for nearness. Handles tuple-shapes
// via recursion. shape_index is the ShapeIndex of expected (or actual)
// currently being compared.
Status NearHelper(const LiteralSlice& expected, const LiteralSlice& actual,
                  const ErrorSpec& error, bool detailed_message,
                  const MiscompareCallback& miscompare_callback,
                  const ShapeIndex& shape_index) {
  TF_RETURN_IF_ERROR(EqualShapes(expected.shape(), actual.shape()));

  if (ShapeUtil::IsTuple(expected.shape())) {
    Status return_status;
    for (int64 i = 0; i < ShapeUtil::TupleElementCount(expected.shape()); ++i) {
      const auto expected_element = LiteralSlice(expected, {i});
      const auto actual_element = LiteralSlice(actual, {i});
      ShapeIndex element_index = shape_index;
      element_index.push_back(i);
      Status res =
          NearHelper(expected_element, actual_element, error, detailed_message,
                     miscompare_callback, element_index);
      if (!res.ok()) {
        string err_message = Printf("\nArray at shape index %s%s",
                                    element_index.ToString().c_str(),
                                    res.error_message().c_str());
        if (return_status.ok()) {
          return_status = res;
        } else {
          return_status = AppendStatus(return_status, res.error_message());
        }
      }
    }
    if (!return_status.ok() && shape_index.empty()) {
      // Emit a top-level error message containing the top-level shape in case
      // of mismatch.
      int64 total_elements = RecursiveElementCount(actual.shape());
      return_status = InvalidArgument(
          "\nMismatches in shape %s (%lld elements):\n%s",
          ShapeUtil::HumanString(actual.shape()).c_str(), total_elements,
          return_status.error_message().c_str());
    }
    return return_status;
  }

  if (ShapeUtil::ElementIsFloating(expected.shape()) ||
      ShapeUtil::ElementIsComplex(expected.shape())) {
    switch (expected.shape().element_type()) {
      case BF16:
        return NearComparator<bfloat16>::Compare(
            expected, actual, error, detailed_message, miscompare_callback);
        break;
      case F16:
        return NearComparator<half>::Compare(
            expected, actual, error, detailed_message, miscompare_callback);
        break;
      case F32:
        return NearComparator<float>::Compare(
            expected, actual, error, detailed_message, miscompare_callback);
        break;
      case F64:
        return NearComparator<double>::Compare(
            expected, actual, error, detailed_message, miscompare_callback);
        break;
      case C64:
        return NearComparator<complex64>::Compare(
            expected, actual, error, detailed_message, miscompare_callback);
        break;
      default:
        LOG(FATAL) << "Unsupported primitive type in near comparator: "
                   << PrimitiveType_Name(expected.shape().element_type())
                   << ". Must be floating-point type.";
    }
  }

  // Non-floating point literal.
  return literal_comparison::Equal(expected, actual);
}

}  // namespace

Status EqualShapes(const Shape& expected, const Shape& actual) {
  if (expected.element_type() != actual.element_type()) {
    return InvalidArgument("element type mismatch, want: %s got %s",
                           ShapeUtil::HumanString(expected).c_str(),
                           ShapeUtil::HumanString(actual).c_str());
  }
  if (ShapeUtil::IsTuple(expected)) {
    if (ShapeUtil::TupleElementCount(expected) !=
        ShapeUtil::TupleElementCount(actual)) {
      return InvalidArgument(
          "want tuple element count: %lld got tuple element count: %lld",
          ShapeUtil::TupleElementCount(expected),
          ShapeUtil::TupleElementCount(actual));
    }
    for (int i = 0; i < expected.tuple_shapes_size(); ++i) {
      Status result =
          EqualShapes(expected.tuple_shapes(i), actual.tuple_shapes(i));
      if (!result.ok()) {
        return AppendStatus(result, StrCat("mismatch in tuple index", i));
      }
    }
  } else if (ShapeUtil::IsArray(expected)) {
    if (ShapeUtil::Rank(expected) != ShapeUtil::Rank(actual)) {
      return InvalidArgument("want rank of %s got rank of %s",
                             ShapeUtil::HumanString(expected).c_str(),
                             ShapeUtil::HumanString(actual).c_str());
    }
    if (expected.element_type() != actual.element_type()) {
      return InvalidArgument(
          "mismatch in primitive type %s vs %s",
          PrimitiveType_Name(expected.element_type()).c_str(),
          PrimitiveType_Name(actual.element_type()).c_str());
    }
    if (expected.dimensions_size() != actual.dimensions_size()) {
      return InvalidArgument("want dimensions_size %d got dimensions_size %d",
                             expected.dimensions_size(),
                             actual.dimensions_size());
    }
    for (int i = 0; i < expected.dimensions_size(); ++i) {
      if (expected.dimensions(i) != actual.dimensions(i)) {
        return InvalidArgument(
            "mismatch in dimension #%d expected: %s actual: %s", i,
            ShapeUtil::HumanString(expected).c_str(),
            ShapeUtil::HumanString(actual).c_str());
      }
    }
  }
  // Non-array, non-tuple shapes are trivially equivalent.
  return Status::OK();
}

Status Equal(const LiteralSlice& expected, const LiteralSlice& actual) {
  VLOG(1) << "expected:";
  XLA_VLOG_LINES(1, expected.ToString());
  VLOG(1) << "actual:";
  XLA_VLOG_LINES(1, actual.ToString());

  TF_RETURN_IF_ERROR(EqualShapes(expected.shape(), actual.shape()));
  std::vector<int64> multi_index(expected.shape().dimensions_size(), 0);
  Status result;
  switch (expected.shape().element_type()) {
    case PRED:
      result = Equal<bool>(expected, actual, &multi_index, 0);
      break;
    case U8:
      result = Equal<uint8>(expected, actual, &multi_index, 0);
      break;
    case S32:
      result = Equal<int32>(expected, actual, &multi_index, 0);
      break;
    case S64:
      result = Equal<int64>(expected, actual, &multi_index, 0);
      break;
    case U32:
      result = Equal<uint32>(expected, actual, &multi_index, 0);
      break;
    case U64:
      result = Equal<uint64>(expected, actual, &multi_index, 0);
      break;
    case BF16:
      result = Equal<bfloat16>(expected, actual, &multi_index, 0);
      break;
    case F16:
      result = Equal<half>(expected, actual, &multi_index, 0);
      break;
    case F32:
      result = Equal<float>(expected, actual, &multi_index, 0);
      break;
    case F64:
      result = Equal<double>(expected, actual, &multi_index, 0);
      break;
    case C64:
      result = Equal<complex64>(expected, actual, &multi_index, 0);
      break;
    case TUPLE: {
      for (int i = 0; i < ShapeUtil::TupleElementCount(expected.shape()); ++i) {
        result.Update(
            Equal(LiteralSlice(expected, {i}), LiteralSlice(actual, {i})));
      }
      break;
    }
    default:
      LOG(FATAL)
          << "Unsupported primitive type in LiteralTestUtil::ExpectEqual: "
          << PrimitiveType_Name(expected.shape().element_type());
  }

  if (result.ok()) {
    return Status::OK();
  }

  return AppendStatus(result,
                      tensorflow::strings::Printf(
                          "\nat index: %s\nexpected: %s\nactual:   %s",
                          Literal::MultiIndexAsString(multi_index).c_str(),
                          ToStringTruncated(expected).c_str(),
                          ToStringTruncated(actual).c_str()));
}

Status Near(const LiteralSlice& expected, const LiteralSlice& actual,
            const ErrorSpec& error, bool detailed_message,
            const MiscompareCallback& miscompare_callback) {
  return NearHelper(expected, actual, error, detailed_message,
                    miscompare_callback,
                    /*shape_index=*/{});
}

string ToStringTruncated(const LiteralSlice& literal) {
  return RecursiveElementCount(literal.shape()) < 1000
             ? literal.ToString()
             : "[TRUNCATED, Literal with more than 1000 values]";
}

}  // namespace literal_comparison
}  // namespace xla
