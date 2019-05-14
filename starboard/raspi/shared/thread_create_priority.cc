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

#include "starboard/shared/pthread/thread_create_priority.h"

#include <sched.h>
#include <sys/resource.h>

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace pthread {

#if SB_HAS(THREAD_PRIORITY_SUPPORT)
// Note that use of sched_setscheduler() has been found to be more reliably
// supported than pthread_setschedparam(), so we are using that.

void SetIdleScheduler() {
  struct sched_param thread_sched_param;
  thread_sched_param.sched_priority = 0;
  int result = sched_setscheduler(0, SCHED_IDLE, &thread_sched_param);
  if (result != 0) {
    SB_NOTREACHED();
  }
}

void SetOtherScheduler() {
  struct sched_param thread_sched_param;
  thread_sched_param.sched_priority = 0;
  int result = sched_setscheduler(0, SCHED_OTHER, &thread_sched_param);
  if (result != 0) {
    SB_NOTREACHED();
  }
}

// Here |priority| is a number between >= 0, where the higher the number, the
// higher the priority.  The actual real time priority setting will be:
//
//     std::min(sched_get_priority_min(SCHED_RR) + priority,
//              sched_get_priority_max(SCHED_RR))
//
// If the desired priority is not supported on this platform, we fall back to
// SCHED_OTHER.
void SetRoundRobinScheduler(int priority) {
  // First determine what the system has setup for the min/max priorities.
  int min_priority = sched_get_priority_min(SCHED_RR);
  int max_priority = sched_get_priority_max(SCHED_RR);

  struct rlimit rlimit_rtprio;
  getrlimit(RLIMIT_RTPRIO, &rlimit_rtprio);

  if (rlimit_rtprio.rlim_cur < min_priority) {
    SB_LOG(WARNING) << "Unable to set real time round-robin thread priority "
                    << "because `ulimit -r` is too low ("
                    << rlimit_rtprio.rlim_cur << " < " << min_priority << ").";

    // Fallback to SCHED_OTHER.
    SetOtherScheduler();
  } else {
    struct sched_param thread_sched_param;
    thread_sched_param.sched_priority =
        std::min(min_priority + priority, max_priority);
    int result = sched_setscheduler(0, SCHED_RR, &thread_sched_param);
    if (result != 0) {
      SB_NOTREACHED();
    }
  }
}

void ThreadSetPriority(SbThreadPriority priority) {
  // Use different schedulers according to priority. This is preferred over
  // using SCHED_RR for all threads because the scheduler time slice is too
  // high (defaults to 100ms) for the desired threading behavior.
  switch (priority) {
    case kSbThreadPriorityLowest:
    case kSbThreadPriorityLow:
      SetIdleScheduler();
      break;
    case kSbThreadNoPriority:
    case kSbThreadPriorityNormal:
      SetOtherScheduler();
      break;
    case kSbThreadPriorityHigh:
      SetRoundRobinScheduler(0);
      break;
    case kSbThreadPriorityHighest:
      SetRoundRobinScheduler(1);
      break;
    case kSbThreadPriorityRealTime:
      SetRoundRobinScheduler(2);
      break;
    default:
      SB_NOTREACHED();
      break;
  }
}

#endif  // SB_HAS(THREAD_PRIORITY_SUPPORT)

}  // namespace pthread
}  // namespace shared
}  // namespace starboard
