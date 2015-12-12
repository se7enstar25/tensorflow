/* Copyright 2015 Google Inc. All Rights Reserved.

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

#ifndef TENSORFLOW_LIB_CORE_ERRORS_H_
#define TENSORFLOW_LIB_CORE_ERRORS_H_

#include "tensorflow/core/lib/strings/strcat.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/public/status.h"

namespace tensorflow {
namespace errors {

typedef ::tensorflow::error::Code Code;

// Append some context to an error message.  Each time we append
// context put it on a new line, since it is possible for there
// to be several layers of additional context.
template <typename... Args>
void AppendToMessage(::tensorflow::Status* status, Args... args) {
  *status = ::tensorflow::Status(
      status->code(),
      strings::StrCat(status->error_message(), "\n\t", args...));
}

// For propagating errors when calling a function.
#define TF_RETURN_IF_ERROR(expr)                         \
  do {                                                   \
    const ::tensorflow::Status _status = (expr);         \
    if (TF_PREDICT_FALSE(!_status.ok())) return _status; \
  } while (0)

#define TF_RETURN_WITH_CONTEXT_IF_ERROR(expr, ...)                  \
  do {                                                              \
    ::tensorflow::Status _status = (expr);                          \
    if (TF_PREDICT_FALSE(!_status.ok())) {                          \
      ::tensorflow::errors::AppendToMessage(&_status, __VA_ARGS__); \
      return _status;                                               \
    }                                                               \
  } while (0)

// Convenience functions for generating and using error status.
// Example usage:
//   status.Update(errors::InvalidArgument("The ", foo, " isn't right."));
//   if (errors::IsInvalidArgument(status)) { ... }
//   switch (status.code()) { case error::INVALID_ARGUMENT: ... }

#define DECLARE_ERROR(FUNC, CONST)                           \
  template <typename... Args>                                \
  inline ::tensorflow::Status FUNC(Args... args) {           \
    return ::tensorflow::Status(::tensorflow::error::CONST,  \
                                strings::StrCat(args...));   \
  }                                                          \
  inline bool Is##FUNC(const ::tensorflow::Status& status) { \
    return status.code() == ::tensorflow::error::CONST;      \
  }

DECLARE_ERROR(Cancelled, CANCELLED)
DECLARE_ERROR(InvalidArgument, INVALID_ARGUMENT)
DECLARE_ERROR(NotFound, NOT_FOUND)
DECLARE_ERROR(AlreadyExists, ALREADY_EXISTS)
DECLARE_ERROR(ResourceExhausted, RESOURCE_EXHAUSTED)
DECLARE_ERROR(Unavailable, UNAVAILABLE)
DECLARE_ERROR(FailedPrecondition, FAILED_PRECONDITION)
DECLARE_ERROR(OutOfRange, OUT_OF_RANGE)
DECLARE_ERROR(Unimplemented, UNIMPLEMENTED)
DECLARE_ERROR(Internal, INTERNAL)
DECLARE_ERROR(Aborted, ABORTED)
DECLARE_ERROR(DeadlineExceeded, DEADLINE_EXCEEDED)
DECLARE_ERROR(DataLoss, DATA_LOSS)
DECLARE_ERROR(Unknown, UNKNOWN)
DECLARE_ERROR(PermissionDenied, PERMISSION_DENIED)
DECLARE_ERROR(Unauthenticated, UNAUTHENTICATED)

#undef DECLARE_ERROR

// The CanonicalCode() for non-errors.
using ::tensorflow::error::OK;

// Convenience macros for asserting and handling exceptional conditions.
// Analogous to the CHECK* macros provided by logging.h.
//
// Example use:
// void Compute(OperationContext* context) {
//   OP_REQUIRES(context, context->num_inputs() == 2,
//               errors::InvalidArgument("FooOp requires 2 arguments"));
//   ...
//   Status status = SomeUncertainMethod();
//   OP_REQUIRES_OK(context, status);
//   ...
// }

// Declares an op deprecated, and illegal starting at GraphDef version VERSION
#define OP_DEPRECATED(CTX, VERSION)                                            \
  if ((CTX)->graph_def_version() >= (VERSION)) {                               \
    ::tensorflow::Status _s(::tensorflow::errors::Unimplemented(               \
        "Op ", (CTX)->op_def().name(),                                         \
        " is not available in GraphDef version ", (CTX)->graph_def_version(),  \
        ". It has been removed in version ", (VERSION), "."));                 \
    VLOG(1) << _s;                                                             \
    (CTX)->SetStatus(_s);                                                      \
    return;                                                                    \
  } else {                                                                     \
    LOG(WARNING) << "Op is deprecated."                                        \
                 << " It will cease to work in GraphDef version " << (VERSION) \
                 << ".";                                                       \
  }

#define OP_REQUIRES(CTX, EXP, STATUS) \
  if (!(EXP)) {                       \
    ::tensorflow::Status _s(STATUS);  \
    VLOG(1) << _s;                    \
    (CTX)->SetStatus(_s);             \
    return;                           \
  }

#define OP_REQUIRES_OK(CTX, STATUS)  \
  do {                               \
    ::tensorflow::Status _s(STATUS); \
    if (!_s.ok()) {                  \
      LOG(WARNING) << _s;            \
      (CTX)->SetStatus(_s);          \
      return;                        \
    }                                \
  } while (0)

#define OP_REQUIRES_ASYNC(CTX, EXP, STATUS, CALLBACK) \
  if (!(EXP)) {                                       \
    ::tensorflow::Status _s(STATUS);                  \
    VLOG(1) << _s;                                    \
    (CTX)->SetStatus(_s);                             \
    (CALLBACK)();                                     \
    return;                                           \
  }

#define OP_REQUIRES_OK_ASYNC(CTX, STATUS, CALLBACK) \
  do {                                              \
    ::tensorflow::Status _s(STATUS);                \
    if (!_s.ok()) {                                 \
      LOG(WARNING) << _s;                           \
      (CTX)->SetStatus(_s);                         \
      (CALLBACK)();                                 \
      return;                                       \
    }                                               \
  } while (0)

}  // namespace errors
}  // namespace tensorflow

#endif  // TENSORFLOW_LIB_CORE_ERRORS_H_
