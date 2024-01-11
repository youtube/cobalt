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

#include "starboard/shared/modular/posix_time_wrappers.h"

#include "starboard/log.h"

int __wrap_clock_gettime(int /*clockid_t */ musl_clock_id,
                         struct musl_timespec* mts) {
  if (!mts) {
    return -1;
  }
  clockid_t clock_id;  // The type from platform toolchain.
  // Map the MUSL clock_id constants to platform constants.
  switch (musl_clock_id) {
    case MUSL_CLOCK_REALTIME:
      clock_id = CLOCK_REALTIME;
      break;
    case MUSL_CLOCK_MONOTONIC:
      clock_id = CLOCK_MONOTONIC;
      break;
    case MUSL_CLOCK_PROCESS_CPUTIME_ID:
      clock_id = CLOCK_PROCESS_CPUTIME_ID;
      break;
    case MUSL_CLOCK_THREAD_CPUTIME_ID:
      clock_id = CLOCK_THREAD_CPUTIME_ID;
      break;
    default:
      clock_id = CLOCK_REALTIME;
      SbLog(kSbLogPriorityError,
            "Unsuppored clock_id used in clock_gettime() - defaulting to "
            "CLOCK_REALTIME.");
  }
  struct timespec ts;  // The type from platform toolchain.
  int retval = clock_gettime(clock_id, &ts);
  mts->tv_sec = ts.tv_sec;
  mts->tv_nsec = ts.tv_nsec;
  return retval;
}

int __wrap_gettimeofday(struct musl_timeval* mtv, void* tzp) {
  if (!mtv) {
    return -1;
  }
  struct timeval tv;  // The type from platform toolchain.
  int retval = gettimeofday(&tv, NULL);
  mtv->tv_sec = tv.tv_sec;
  mtv->tv_usec = tv.tv_usec;
  return retval;
}
