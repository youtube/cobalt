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

int __abi_wrap_clock_gettime(int /* clockid_t */ musl_clock_id,
                             struct musl_timespec* mts) {
  if (!mts) {
    return -1;
  }

  // The type from platform toolchain.
  // Map the MUSL clock_id constants to platform constants.
  clockid_t clock_id = musl_clock_id_to_clock_id(musl_clock_id);
  struct timespec ts;  // The type from platform toolchain.
  int retval = clock_gettime(clock_id, &ts);
  mts->tv_sec = ts.tv_sec;
  mts->tv_nsec = ts.tv_nsec;
  return retval;
}

int64_t __abi_wrap_time(int64_t* /* time_t* */ musl_tloc) {
  time_t t = time(NULL);  // The type from platform toolchain (may be 32-bits).
  int64_t retval = static_cast<int64_t>(t);
  if (musl_tloc) {
    *musl_tloc = retval;
  }
  return retval;
}

struct musl_tm* __abi_wrap_gmtime_r(const int64_t* /* time_t* */ musl_timer,
                                    struct musl_tm* musl_result) {
  if (!musl_timer || !musl_result) {
    return NULL;
  }
  time_t t = static_cast<time_t>(*musl_timer);  // Platform type may be 32-bits.
  struct tm tm;  // The type from platform toolchain.
  if (!gmtime_r(&t, &tm)) {
    return NULL;
  }
  musl_result->tm_sec = tm.tm_sec;
  musl_result->tm_min = tm.tm_min;
  musl_result->tm_hour = tm.tm_hour;
  musl_result->tm_mday = tm.tm_mday;
  musl_result->tm_mon = tm.tm_mon;
  musl_result->tm_year = tm.tm_year;
  musl_result->tm_wday = tm.tm_wday;
  musl_result->tm_yday = tm.tm_yday;
  musl_result->tm_isdst = tm.tm_isdst;
  return musl_result;
}
