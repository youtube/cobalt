// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <sched.h>
#include <sys/resource.h>

#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace {
int SbPriorityToNice(SbThreadPriority priority) {
  switch (priority) {
    case kSbThreadPriorityLowest:
      return 19;
    case kSbThreadPriorityLow:
      return 10;
    case kSbThreadNoPriority:
    case kSbThreadPriorityNormal:
      return 0;
    case kSbThreadPriorityHigh:
      return -8;
    case kSbThreadPriorityHighest:
      return -16;
    case kSbThreadPriorityRealTime:
      return -19;
  }
  return 0;
}

SbThreadPriority NiceToSbPriority(int nice) {
  if (nice == 19) {
    return kSbThreadPriorityLowest;
  }
  if (nice > 0 && nice < 19) {
    return kSbThreadPriorityLow;
  }
  if (nice == 0) {
    return kSbThreadPriorityNormal;
  }
  if (nice >= -8 && nice < 0) {
    return kSbThreadPriorityHigh;
  }
  if (nice > -19 && nice < -8) {
    return kSbThreadPriorityHighest;
  }
  if (nice == -19) {
    return kSbThreadPriorityRealTime;
  }
  return kSbThreadPriorityNormal;
}

}  // namespace

bool SbThreadSetPriority(SbThreadPriority priority) {
  int res = setpriority(PRIO_PROCESS, 0, SbPriorityToNice(priority));
  return res == 0;
}

bool SbThreadGetPriority(SbThreadPriority* priority) {
  errno = 0;
  int ret = getpriority(PRIO_PROCESS, 0);
  if (ret == -1 && errno != 0) {
    return false;
  }
  *priority = NiceToSbPriority(ret);
  return true;
}
