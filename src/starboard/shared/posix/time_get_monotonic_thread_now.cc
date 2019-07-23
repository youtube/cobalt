// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/time.h"

#include <time.h>

#include "starboard/common/log.h"
#include "starboard/shared/posix/time_internal.h"

SbTimeMonotonic SbTimeGetMonotonicThreadNow() {
  struct timespec time;
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) != 0) {
    SB_NOTREACHED() << "clock_gettime(CLOCK_THREAD_CPUTIME_ID) failed.";
    return 0;
  }

  return FromTimespecDelta(&time);
}
