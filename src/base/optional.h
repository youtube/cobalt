/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BASE_OPTIONAL_H_
#define BASE_OPTIONAL_H_

#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "starboard/common/optional.h"

namespace base {
using starboard::nullopt_t;
using starboard::nullopt;
using starboard::in_place_t;
using starboard::in_place;
using starboard::optional;
using starboard::make_optional;
}  // namespace base

#endif  // BASE_OPTIONAL_H_
