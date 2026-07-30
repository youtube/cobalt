// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_IAMF_TOOLS_STRIP_ERRORS_H_
#define THIRD_PARTY_IAMF_TOOLS_STRIP_ERRORS_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"

namespace absl {

inline Status StripInvalidArgumentError(size_t) {
  return InvalidArgumentError("");
}

inline Status StripUnimplementedError(size_t) {
  return UnimplementedError("");
}

inline Status StripUnknownError(size_t) {
  return UnknownError("");
}

inline Status StripInternalError(size_t) {
  return InternalError("");
}

inline Status StripOutOfRangeError(size_t) {
  return OutOfRangeError("");
}

inline Status StripNotFoundError(size_t) {
  return NotFoundError("");
}

inline Status StripFailedPreconditionError(size_t) {
  return FailedPreconditionError("");
}

inline Status StripResourceExhaustedError(size_t) {
  return ResourceExhaustedError("");
}

inline Status StripAbortedError(size_t) {
  return AbortedError("");
}

inline Status StripCancelledError(size_t) {
  return CancelledError("");
}

inline Status StripDataLossError(size_t) {
  return DataLossError("");
}

inline Status StripDeadlineExceededError(size_t) {
  return DeadlineExceededError("");
}

inline Status StripPermissionDeniedError(size_t) {
  return PermissionDeniedError("");
}

inline Status StripUnauthenticatedError(size_t) {
  return UnauthenticatedError("");
}

inline Status StripUnavailableError(size_t) {
  return UnavailableError("");
}

}  // namespace absl

using absl::StripInvalidArgumentError;
using absl::StripUnimplementedError;
using absl::StripUnknownError;
using absl::StripInternalError;
using absl::StripOutOfRangeError;
using absl::StripNotFoundError;
using absl::StripFailedPreconditionError;
using absl::StripResourceExhaustedError;
using absl::StripAbortedError;
using absl::StripCancelledError;
using absl::StripDataLossError;
using absl::StripDeadlineExceededError;
using absl::StripPermissionDeniedError;
using absl::StripUnauthenticatedError;
using absl::StripUnavailableError;

#undef InvalidArgumentError
#define InvalidArgumentError(...) StripInvalidArgumentError(sizeof((__VA_ARGS__)))

#undef UnimplementedError
#define UnimplementedError(...) StripUnimplementedError(sizeof((__VA_ARGS__)))

#undef UnknownError
#define UnknownError(...) StripUnknownError(sizeof((__VA_ARGS__)))

#undef InternalError
#define InternalError(...) StripInternalError(sizeof((__VA_ARGS__)))

#undef OutOfRangeError
#define OutOfRangeError(...) StripOutOfRangeError(sizeof((__VA_ARGS__)))

#undef NotFoundError
#define NotFoundError(...) StripNotFoundError(sizeof((__VA_ARGS__)))

#undef FailedPreconditionError
#define FailedPreconditionError(...) StripFailedPreconditionError(sizeof((__VA_ARGS__)))

#undef ResourceExhaustedError
#define ResourceExhaustedError(...) StripResourceExhaustedError(sizeof((__VA_ARGS__)))

#undef AbortedError
#define AbortedError(...) StripAbortedError(sizeof((__VA_ARGS__)))

#undef CancelledError
#define CancelledError(...) StripCancelledError(sizeof((__VA_ARGS__)))

#undef DataLossError
#define DataLossError(...) StripDataLossError(sizeof((__VA_ARGS__)))

#undef DeadlineExceededError
#define DeadlineExceededError(...) StripDeadlineExceededError(sizeof((__VA_ARGS__)))

#undef PermissionDeniedError
#define PermissionDeniedError(...) StripPermissionDeniedError(sizeof((__VA_ARGS__)))

#undef UnauthenticatedError
#define UnauthenticatedError(...) StripUnauthenticatedError(sizeof((__VA_ARGS__)))

#undef UnavailableError
#define UnavailableError(...) StripUnavailableError(sizeof((__VA_ARGS__)))

#endif  // THIRD_PARTY_IAMF_TOOLS_STRIP_ERRORS_H_
