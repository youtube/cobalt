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

#include "starboard/shared/modular/starboard_layer_posix_time_abi_wrappers.h"

#include <errno.h>

#include "starboard/shared/modular/starboard_layer_posix_errno_abi_wrappers.h"

int __abi_wrap_clock_gettime(int /* clockid_t */ musl_clock_id,
                             struct musl_timespec* mts) {
  if (!mts) {
    errno = EFAULT;
    return -1;
  }

  // The type from platform toolchain.
  // Map the MUSL clock_id constants to platform constants.
  clockid_t clock_id = musl_clock_id_to_clock_id(musl_clock_id);
  if (clock_id == MUSL_CLOCK_INVALID) {
    errno = EINVAL;
    return -1;
  }
  struct timespec ts;  // The type from platform toolchain.
  int retval = clock_gettime(clock_id, &ts);
  mts->tv_sec = static_cast<int64_t>(ts.tv_sec);
  mts->tv_nsec = ts.tv_nsec;
  return retval;
}

int __abi_wrap_clock_nanosleep(int /* clockid_t */ musl_clock_id,
                               int flags,
                               const struct musl_timespec* mts,
                               struct musl_timespec* mremain) {
  if (!mts) {
    return MUSL_EFAULT;
  }
  if (mts->tv_sec < 0) {
    // Invalid relative time.
    return MUSL_EINVAL;
  }
  if ((flags & ~MUSL_TIMER_ABSTIME) != 0) {
    // Invalid flags.
    return MUSL_EINVAL;
  }
  struct timespec ts;
  struct timespec remain;
  ts.tv_sec = mts->tv_sec;
  ts.tv_nsec = mts->tv_nsec;
  if (musl_clock_id == MUSL_CLOCK_THREAD_CPUTIME_ID) {
    return MUSL_EINVAL;
  }
  clockid_t clock_id = musl_clock_id_to_clock_id(musl_clock_id);
  if (clock_id == MUSL_CLOCK_INVALID) {
    return MUSL_EINVAL;
  }
  int retval =
      clock_nanosleep(clock_id, musl_nanosleep_flags_to_nanosleep_flags(flags),
                      &ts, mremain ? &remain : nullptr);
  if (mremain && !((retval == EINTR) && (flags & MUSL_TIMER_ABSTIME))) {
    mremain->tv_sec = static_cast<int64_t>(remain.tv_sec);
    mremain->tv_nsec = remain.tv_nsec;
  }
  return errno_to_musl_errno(retval);
}
