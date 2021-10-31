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

#if SB_API_VERSION >= 12 || SB_HAS(THREAD_PRIORITY_SUPPORT)
// This is the maximum priority that will be passed to SetRoundRobinScheduler().
const int kMaxRoundRobinPriority = 2;

// Permissions are needed to allow usage non-default thread schedulers and
// thread priorities. Specifically, /etc/security/limits.conf should set
// "rtprio" and "nice" limits. "rtprio" will allow use of the real-time
// schedulers, and "nice" will allow use of nice priorities as well as allow
// SCHED_IDLE threads to increase their priority. If the user is 'pi', then
// limits.conf should have the following lines:
//   @pi hard rtprio 99
//   @pi soft rtprio 99
//   @pi hard nice -20
//   @pi soft nice -20
const char kSchedulerErrorMessage[] = "Unable to set scheduler. Please update "
    "limits.conf to set 'rtprio' limit to 99 and 'nice' limit to -20.";

// Note that use of sched_setscheduler() has been found to be more reliably
// supported than pthread_setschedparam(), so we are using that.

void SetIdleScheduler() {
  struct sched_param thread_sched_param;
  thread_sched_param.sched_priority = 0;
  int result = sched_setscheduler(0, SCHED_IDLE, &thread_sched_param);
  SB_CHECK(result == 0) << kSchedulerErrorMessage;
}

void SetOtherScheduler() {
  struct sched_param thread_sched_param;
  thread_sched_param.sched_priority = 0;
  int result = sched_setscheduler(0, SCHED_OTHER, &thread_sched_param);
  SB_CHECK(result == 0) << kSchedulerErrorMessage;
}

// Here |priority| is a number >= 0, where the higher the number, the
// higher the priority.
//
// If real time priorities are not allowed, then fall back to SCHED_OTHER.
// Otherwise, use SCHED_RR with the desired priority (or RLIMIT_RTPRIO --
// whichever is lower).
void SetRoundRobinScheduler(int priority) {
  // Determine the minimum and maximum priorities that will be used.
  int min_priority = sched_get_priority_min(SCHED_RR);
  int max_used_priority = min_priority + kMaxRoundRobinPriority;

  struct rlimit rlimit_rtprio;
  getrlimit(RLIMIT_RTPRIO, &rlimit_rtprio);

  if (rlimit_rtprio.rlim_cur < min_priority) {
    // Can't use the real-time scheduler at all. Use SCHED_OTHER instead.
    // Performance will be noticeably worse than ideal.
    SB_LOG(ERROR) << "Unable to use real-time round-robin scheduler because "
                  << "the maximum real-time scheduling priority is too low ("
                  << rlimit_rtprio.rlim_cur << " < " << max_used_priority
                  <<"). Update setting using `ulimit -r` or limits.conf file.";
    SetOtherScheduler();
    return;
  }

  if (rlimit_rtprio.rlim_cur < max_used_priority) {
    // The maximum desired priority will be clamped.
    // Performance will be slightly worse than ideal.
    SB_LOG(WARNING) << "The maximum real-time scheduling priority is too low ("
                    << rlimit_rtprio.rlim_cur << " < " << max_used_priority
                    << "). Update setting using `ulimit -r` or limits.conf "
                    << "file.";
  }

  struct sched_param thread_sched_param;
  thread_sched_param.sched_priority =
      std::min(min_priority + priority,
               static_cast<int>(rlimit_rtprio.rlim_cur));
  int result = sched_setscheduler(0, SCHED_RR, &thread_sched_param);
  SB_CHECK(result == 0) << kSchedulerErrorMessage;
}

void ThreadSetPriority(SbThreadPriority priority) {
  if (!kSbHasThreadPrioritySupport)
    return;

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
      SetRoundRobinScheduler(kMaxRoundRobinPriority);
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
