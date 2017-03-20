// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/log.h"

namespace starboard {
namespace shared {
namespace pthread {

#if SB_HAS(THREAD_PRIORITY_SUPPORT)
void ThreadSetPriority(SbThreadPriority priority) {
  // Note that use of sched_setscheduler() has been found to be more reliably
  // supported than pthread_setschedparam(), so we are using that.

  // Use different schedulers according to priority. This is preferred over
  // using SCHED_RR for all threads because the scheduler time slice is too
  // high (defaults to 100ms) for the desired threading behavior.
  struct sched_param thread_sched_param;
  int result = 0;

  thread_sched_param.sched_priority = 0;
  switch (priority) {
    case kSbThreadPriorityLowest:
    case kSbThreadPriorityLow:
      result = sched_setscheduler(0, SCHED_IDLE, &thread_sched_param);
      break;
    case kSbThreadNoPriority:
    case kSbThreadPriorityNormal:
      result = sched_setscheduler(0, SCHED_OTHER, &thread_sched_param);
      break;
    case kSbThreadPriorityHigh:
      thread_sched_param.sched_priority = 1;
      result = sched_setscheduler(0, SCHED_RR, &thread_sched_param);
      break;
    case kSbThreadPriorityHighest:
      thread_sched_param.sched_priority = 2;
      result = sched_setscheduler(0, SCHED_RR, &thread_sched_param);
      break;
    case kSbThreadPriorityRealTime:
      thread_sched_param.sched_priority = 3;
      result = sched_setscheduler(0, SCHED_RR, &thread_sched_param);
      break;
    default:
      SB_NOTREACHED();
      break;
  }

  if (result != 0) {
    SB_NOTREACHED();
  }
}
#endif  // SB_HAS(THREAD_PRIORITY_SUPPORT)

}  // namespace pthread
}  // namespace shared
}  // namespace starboard
