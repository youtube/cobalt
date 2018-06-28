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

#ifndef STARBOARD_SHARED_POSIX_TIME_INTERNAL_H_
#define STARBOARD_SHARED_POSIX_TIME_INTERNAL_H_

#include <sys/time.h>
#include <time.h>

#include "starboard/shared/internal_only.h"
#include "starboard/time.h"

namespace {
const int64_t kMillisecondsPerSecond = kSbTimeSecond / kSbTimeMillisecond;
const int64_t kNanosecondsPerMicrosecond = 1000;

inline SbTime FromMilliseconds(int64_t ms) {
  return ms * kSbTimeMillisecond;
}

inline SbTime FromSeconds(int64_t secs) {
  return secs * kSbTimeSecond;
}

inline SbTime FromNanoseconds(int64_t ns) {
  return ns / kNanosecondsPerMicrosecond;
}

// Converts a timespec representing a duration into microseconds.
inline int64_t FromTimespecDelta(struct timespec* time) {
  return FromSeconds(static_cast<int64_t>(time->tv_sec)) +
         FromNanoseconds(static_cast<int64_t>(time->tv_nsec));
}

// Converts the number of microseconds in |time_us| into a POSIX timespec,
// placing the result in |out_time|.
inline void ToTimespec(struct timespec* out_time, int64_t time_us) {
  out_time->tv_sec = time_us / kSbTimeSecond;
  int64_t remainder_us = time_us - (out_time->tv_sec * kSbTimeSecond);
  out_time->tv_nsec = remainder_us * kNanosecondsPerMicrosecond;
}

// Converts a timeval (relative to POSIX epoch) into microseconds since the
// Windows epoch (1601).
inline int64_t FromTimeval(const struct timeval* time) {
  return SbTimeFromPosix(FromSeconds(static_cast<int64_t>(time->tv_sec)) +
                         time->tv_usec);
}

// Converts a number of microseconds into a timeval.
inline void ToTimevalDuration(int64_t microseconds, struct timeval* out_time) {
  out_time->tv_sec = microseconds / kSbTimeSecond;
  out_time->tv_usec = microseconds % kSbTimeSecond;
}

// Converts a time_t (relative to POSIX epoch) into microseconds since the
// Windows epoch (1601).
inline int64_t FromTimeT(time_t time) {
  return SbTimeFromPosix(FromSeconds(time));
}
}  // namespace

#endif  // STARBOARD_SHARED_POSIX_TIME_INTERNAL_H_
