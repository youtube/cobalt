/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RLWE_STATUSOR_H_
#define RLWE_STATUSOR_H_

#include "absl/status/statusor.h"
#include "third_party/shell-encryption/base/shell_encryption_export.h"

namespace rlwe {

template <typename T>
using StatusOr = absl::StatusOr<T>;

}  // namespace rlwe

#endif  // RLWE_STATUSOR_H_
