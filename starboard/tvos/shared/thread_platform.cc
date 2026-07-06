/*
 * Copyright 2026 The Cobalt Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "starboard/common/thread_platform.h"

#include <pthread.h>
#include <sched.h>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"

namespace starboard {
namespace {

// Returns a value between 0 and 1 that is used by SetThreadPriority() to scale
// the desired thread priority between what sched_get_priority_min() and
// sched_get_priority_max() return.
float ThreadPriorityToRelativeSchedPriority(ThreadPriority priority) {
  switch (priority) {
    case ThreadPriority::kLowest:
      return 0.1f;
    case ThreadPriority::kLow:
      return 0.3f;
    case ThreadPriority::kNoPriority:
    case ThreadPriority::kNormal:
      return 0.5f;
    case ThreadPriority::kHigh:
      return 0.7f;
    case ThreadPriority::kHighest:
      return 0.9f;
    case ThreadPriority::kRealTime:
      return 0.99f;
  }
  SB_NOTREACHED();
}

}  // namespace

void SetCurrentThreadName(const char* name) {
  pthread_setname_np(name);
}

bool SetCurrentThreadPriority(ThreadPriority priority) {
  const float relative_priority =
      ThreadPriorityToRelativeSchedPriority(priority);

  int policy;
  struct sched_param param;

  // Get the current thread scheduling parameters. Only the thread priority
  // will be changed.
  SB_CHECK_EQ(pthread_getschedparam(pthread_self(), &policy, &param), 0);
  const int min_priority = sched_get_priority_min(policy);
  const int max_priority = sched_get_priority_max(policy);

  // Thread priority set with pthread does not appear to have the same range
  // as NSThread priorities. A pthread set to sched_get_priority_max() won't
  // have NSThread.threadPriority == 1.0. Consider switching to NSThread if a
  // higher priority is required.
  param.sched_priority = static_cast<int>(
      min_priority + relative_priority * (max_priority - min_priority));
  return pthread_setschedparam(pthread_self(), policy, &param) == 0;
}

void TerminateOnThread() {}

}  // namespace starboard
