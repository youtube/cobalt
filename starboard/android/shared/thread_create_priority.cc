// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/pthread/thread_create_priority.h"

#include <sched.h>
#include <sys/resource.h>

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace pthread {

#if SB_API_VERSION >= 12 || SB_HAS(THREAD_PRIORITY_SUPPORT)

void SetNiceValue(int nice) {
  int result = setpriority(PRIO_PROCESS, 0, nice);
  if (result != 0) {
    SB_NOTREACHED();
  }
}

void ThreadSetPriority(SbThreadPriority priority) {
  if (!kSbHasThreadPrioritySupport)
    return;

  // Nice value settings are selected from looking at:
  //   https://android.googlesource.com/platform/frameworks/native/+/jb-dev/include/utils/ThreadDefs.h#35
  switch (priority) {
    case kSbThreadPriorityLowest:
      SetNiceValue(19);
      break;
    case kSbThreadPriorityLow:
      SetNiceValue(10);
      break;
    case kSbThreadNoPriority:
    case kSbThreadPriorityNormal:
      SetNiceValue(0);
      break;
    case kSbThreadPriorityHigh:
      SetNiceValue(-8);
      break;
    case kSbThreadPriorityHighest:
      SetNiceValue(-16);
      break;
    case kSbThreadPriorityRealTime:
      SetNiceValue(-19);
      break;
    default:
      SB_NOTREACHED();
      break;
  }
}

#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(THREAD_PRIORITY_SUPPORT)
}  // namespace pthread
}  // namespace shared
}  // namespace starboard
