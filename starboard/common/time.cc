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

#include "starboard/common/time.h"

#include <sys/time.h>
#include <time.h>

#include "starboard/common/log.h"

namespace starboard {

int64_t CurrentMonotonicTime() {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    SB_NOTREACHED() << "clock_gettime(CLOCK_MONOTONIC) failed.";
    return 0;
  }
  return (static_cast<int64_t>(ts.tv_sec) * 1000000) +
         (static_cast<int64_t>(ts.tv_nsec) / 1000);
}

int64_t CurrentMonotonicThreadTime() {
  struct timespec ts;
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
    // This is expected to happen on some systems, like Windows.
    return 0;
  }
  return (static_cast<int64_t>(ts.tv_sec) * 1000000) +
         (static_cast<int64_t>(ts.tv_nsec) / 1000);
}

int64_t CurrentPosixTime() {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    SB_NOTREACHED() << "Could not determine time of day.";
    return 0;
  }
  return (static_cast<int64_t>(tv.tv_sec) * 1000000) + tv.tv_usec;
}

}  // namespace starboard
