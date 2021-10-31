// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Time module
//
// Provides access to system time and timers.

#ifndef STARBOARD_TIME_H_
#define STARBOARD_TIME_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// The number of microseconds since the epoch of January 1, 1601 UTC, or the
// number of microseconds between two times. Always microseconds, ALWAYS UTC.
typedef int64_t SbTime;

// A number of microseconds from some point. The main property of this time is
// that it increases monotonically. It should also be as high-resolution a timer
// as we can get on a platform. So, it is good for measuring the time between
// two calls without worrying about a system clock adjustment.  It's not good
// for getting the wall clock time.
typedef int64_t SbTimeMonotonic;

// How many nanoseconds in one SbTime unit (microseconds).
#define kSbTimeNanosecondsPerMicrosecond ((SbTime)1000)

// One millisecond in SbTime units (microseconds).
#define kSbTimeMillisecond ((SbTime)1000)

// One second in SbTime units (microseconds).
#define kSbTimeSecond (kSbTimeMillisecond * 1000)

// One minute in SbTime units (microseconds).
#define kSbTimeMinute (kSbTimeSecond * 60)

// One hour in SbTime units (microseconds).
#define kSbTimeHour (kSbTimeMinute * 60)

// One day in SbTime units (microseconds).
#define kSbTimeDay (kSbTimeHour * 24)

// The maximum value of an SbTime.
#define kSbTimeMax (kSbInt64Max)

// A term that can be added to an SbTime to convert it into the number of
// microseconds since the POSIX epoch.
#define kSbTimeToPosixDelta (SB_INT64_C(-11644473600) * kSbTimeSecond)

// Converts |SbTime| into microseconds from the POSIX epoch.
//
// |time|: A time that is either measured in microseconds since the epoch of
//   January 1, 1601, UTC, or that measures the number of microseconds
//   between two times.
static SB_C_FORCE_INLINE int64_t SbTimeToPosix(SbTime time) {
  return time + kSbTimeToPosixDelta;
}

// Converts microseconds from the POSIX epoch into an |SbTime|.
//
// |time|: A time that measures the number of microseconds since
//   January 1, 1970, 00:00:00, UTC.
static SB_C_FORCE_INLINE SbTime SbTimeFromPosix(int64_t time) {
  return time - kSbTimeToPosixDelta;
}

// Safely narrows a number from a more precise unit to a less precise one. This
// function rounds negative values toward negative infinity.
static SB_C_FORCE_INLINE int64_t SbTimeNarrow(int64_t time, int64_t divisor) {
  return time >= 0 ? time / divisor : (time - divisor + 1) / divisor;
}

// Gets the current system time as an |SbTime|.
SB_EXPORT SbTime SbTimeGetNow();

// Gets a monotonically increasing time representing right now.
SB_EXPORT SbTimeMonotonic SbTimeGetMonotonicNow();

#if SB_API_VERSION >= 12 || SB_HAS(TIME_THREAD_NOW)

#if SB_API_VERSION >= 12
// Returns whether the current platform supports time thread now
SB_EXPORT bool SbTimeIsTimeThreadNowSupported();
#endif

// Gets a monotonically increasing time representing how long the current
// thread has been in the executing state (i.e. not pre-empted nor waiting
// on an event). This is not necessarily total time and is intended to allow
// measuring thread execution time between two timestamps. If this is not
// available then SbTimeGetMonotonicNow() should be used.
SB_EXPORT SbTimeMonotonic SbTimeGetMonotonicThreadNow();

#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(TIME_THREAD_NOW)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_TIME_H_
