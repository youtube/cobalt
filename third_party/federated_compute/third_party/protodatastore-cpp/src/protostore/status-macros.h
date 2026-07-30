// Copyright (C) 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PROTOSTORE_STATUS_MACROS_H_
#define PROTOSTORE_STATUS_MACROS_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace protostore {
namespace internal {
/** Functions to assist with PDS_RETURN_IF_ERROR() */
inline const absl::Status AsStatus(const absl::Status& status) { return status; }
template <typename T>
inline const absl::Status AsStatus(const absl::StatusOr<T>& status_or) {
  return status_or.status();
}
}  // namespace internal

/**
 * Macro which allows to check for a Status (or StatusOr) and return from the
 * current method if not OK. Example:
 *
 *     Status DoSomething() {
 *       PDS_RETURN_IF_ERROR(Step1());
 *       PDS_RETURN_IF_ERROR(Step2ReturningStatusOr().status());
 *       return PDS_STATUS(OK);
 *     }
 */
#define PDS_RETURN_IF_ERROR(expr)                     \
  do {                                                \
    absl::Status __status = internal::AsStatus(expr); \
    if (__status.code() != absl::StatusCode::kOk) {   \
      return (__status);                              \
    }                                                 \
  } while (false)

/**
 * Macro which allows to check for a StatusOr and return it's status if not OK,
 * otherwise assign the value in the StatusOr to variable or declaration. Usage:
 *
 *     StatusOr<bool> DoSomething() {
 *       PDS_ASSIGN_OR_RETURN(auto value, TryComputeSomething());
 *       if (!value) {
 *         PDS_ASSIGN_OR_RETURN(value, TryComputeSomethingElse());
 *       }
 *       return value;
 *     }
 */
#define PDS_ASSIGN_OR_RETURN(lhs, expr) \
  _PDS_ASSIGN_OR_RETURN_1(              \
      _PDS_ASSIGN_OR_RETURN_CONCAT(statusor_for_aor, __LINE__), lhs, expr)

#define _PDS_ASSIGN_OR_RETURN_1(statusor, lhs, expr) \
  auto statusor = (expr);                            \
  if (!statusor.ok()) {                              \
    return statusor.status();                        \
  }                                                  \
  lhs = std::move(statusor).value()

// See https://goo.gl/x3iba2 for the reason of this construction.
#define _PDS_ASSIGN_OR_RETURN_CONCAT(x, y) \
  _PDS_ASSIGN_OR_RETURN_CONCAT_INNER(x, y)
#define _PDS_ASSIGN_OR_RETURN_CONCAT_INNER(x, y) x##y

}  // namespace protostore
#endif  // PROTOSTORE_STATUS_MACROS_H_
