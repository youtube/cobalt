// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "base/time/time.h"

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/time/time_override.h"
#include "build/build_config.h"

#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/types.h"

namespace base {

// Time -----------------------------------------------------------------------

namespace subtle {
Time TimeNowIgnoringOverride() {
  return Time() + TimeDelta::FromMicroseconds(
      starboard::PosixTimeToWindowsTime(starboard::CurrentPosixTime()));
}

Time TimeNowFromSystemTimeIgnoringOverride() {
  // Just use TimeNowIgnoringOverride() because it returns the system time.
  return TimeNowIgnoringOverride();
}
}  // namespace subtle

// TimeTicks ------------------------------------------------------------------

namespace subtle {
TimeTicks TimeTicksNowIgnoringOverride() {
  return TimeTicks() + TimeDelta::FromMicroseconds(
      starboard::CurrentMonotonicTime());
}
}  // namespace subtle

// static
TimeTicks::Clock TimeTicks::GetClock() {
  SB_NOTIMPLEMENTED();
  return Clock::LINUX_CLOCK_MONOTONIC;
}

// static
bool TimeTicks::IsConsistentAcrossProcesses() {
  return true;
}

// ThreadTicks ----------------------------------------------------------------

namespace subtle {
ThreadTicks ThreadTicksNowIgnoringOverride() {
  return ThreadTicks() + TimeDelta::FromMicroseconds(
      starboard::CurrentMonotonicThreadTime());
}
}  // namespace subtle

}  // namespace base
