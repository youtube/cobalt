// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_TIME_H_
#define STARBOARD_COMMON_TIME_H_

#include "starboard/configuration.h"
#include "starboard/types.h"

namespace starboard {

// A convenience helper to get the current Monotonic time in microseconds.
int64_t CurrentMonotonicTime();

// A convenience helper to get the current thread-specific Monotonic time in
// microseconds. Returns 0 if the system doesn't support thread-specific clock.
int64_t CurrentMonotonicThreadTime();

// A convenience helper to get the current POSIX system time in microseconds.
int64_t CurrentPosixTime();

// Converts a POSIX microseconds timestamp to a Windows microseconds timestamp.
static SB_C_FORCE_INLINE int64_t PosixTimeToWindowsTime(int64_t posix_time) {
  // Add number of microseconds since Jan 1, 1601 (UTC) until Jan 1, 1970 (UTC).
  return posix_time + 11644473600000000ULL;
}

}  // namespace starboard

#endif  // STARBOARD_COMMON_TIME_H_
