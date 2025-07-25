/*
 * Copyright 2019 Google Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UTIL_STATUS_MACROS_H_
#define UTIL_STATUS_MACROS_H_

#include "third_party/private-join-and-compute/src/chromium_patch.h"
#include "third_party/private-join-and-compute/src/util/status.h"
#include "third_party/private-join-and-compute/src/util/statusor.h"

// Helper macro that checks if the right hand side (rexpression) evaluates to a
// StatusOr with Status OK, and if so assigns the value to the value on the left
// hand side (lhs), otherwise returns the error status. Example:
//   ASSIGN_OR_RETURN(lhs, rexpression);
#define ASSIGN_OR_RETURN(lhs, rexpr)                                          \
  PRIVACY_BLINDERS_ASSIGN_OR_RETURN_IMPL_(                                    \
      PRIVACY_BLINDERS_STATUS_MACROS_IMPL_CONCAT_(status_or_value, __LINE__), \
      lhs, rexpr)

// Internal helper.
#define PRIVACY_BLINDERS_ASSIGN_OR_RETURN_IMPL_(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                                            \
  if (UNLIKELY(!statusor.ok())) {                                     \
    return std::move(statusor).status();                              \
  }                                                                   \
  lhs = std::move(statusor).value()

// Internal helper for concatenating macro values.
#define PRIVACY_BLINDERS_STATUS_MACROS_IMPL_CONCAT_INNER_(x, y) x##y
#define PRIVACY_BLINDERS_STATUS_MACROS_IMPL_CONCAT_(x, y) \
  PRIVACY_BLINDERS_STATUS_MACROS_IMPL_CONCAT_INNER_(x, y)

#endif  // UTIL_STATUS_MACROS_H_
