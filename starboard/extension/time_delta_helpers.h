// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_EXTENSION_TIME_DELTA_HELPERS_H_
#define STARBOARD_EXTENSION_TIME_DELTA_HELPERS_H_

#include "base/time/time.h"

namespace starboard {
namespace extension {

inline constexpr base::TimeDelta kDefaultTimeDelta = base::Microseconds(0);

}  // namespace extension
}  // namespace starboard

#endif  // STARBOARD_EXTENSION_TIME_DELTA_HELPERS_H_
