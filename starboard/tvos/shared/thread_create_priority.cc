// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include <pthread.h>
#include <sched.h>

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/thread.h"

namespace {

bool PthreadToSbThreadPriority(int priority,
                               int min_priority,
                               int max_priority,
                               SbThreadPriority* sb_priority) {
  if (priority < min_priority) {
    return false;
  }
  priority -= min_priority;
  if (max_priority <= min_priority) {
    return false;
  }
  priority /= (max_priority - min_priority);
  if (priority <= 0.1f) {
    *sb_priority = kSbThreadPriorityLowest;
    return true;
  }
  if (priority <= 0.3f) {
    *sb_priority = kSbThreadPriorityLow;
    return true;
  }
  if (priority <= 0.5f) {
    *sb_priority = kSbThreadPriorityNormal;
    return true;
  }
  if (priority <= 0.7f) {
    *sb_priority = kSbThreadPriorityHigh;
    return true;
  }
  if (priority <= 0.9f) {
    *sb_priority = kSbThreadPriorityHighest;
    return true;
  }
  if (priority <= 1.0f) {
    *sb_priority = kSbThreadPriorityRealTime;
    return true;
  }
  return false;
}

}  // namespace

bool SbThreadSetPriority(SbThreadPriority priority) {
  if (!kSbHasThreadPrioritySupport) {
    return false;
  }

  float relative_priority = 0.5f;

  switch (priority) {
    case kSbThreadPriorityLowest:
      relative_priority = 0.1f;
      break;
    case kSbThreadPriorityLow:
      relative_priority = 0.3f;
      break;
    case kSbThreadNoPriority:
    case kSbThreadPriorityNormal:
      relative_priority = 0.5f;
      break;
    case kSbThreadPriorityHigh:
      relative_priority = 0.7f;
      break;
    case kSbThreadPriorityHighest:
      relative_priority = 0.9f;
      break;
    case kSbThreadPriorityRealTime:
      relative_priority = 0.99f;
      break;
    default:
      SB_NOTREACHED();
      break;
  }

  struct sched_param param;
  int policy = SCHED_RR;

  // Get the current thread scheduling parameters. Only the thread priority
  // will be changed.
  SB_CHECK(pthread_getschedparam(pthread_self(), &policy, &param) == 0);
  int min_priority = sched_get_priority_min(policy);
  int max_priority = sched_get_priority_max(policy);

  // Thread priority set with pthread does not appear to have the same range
  // as NSThread priorities. A pthread set to sched_get_priority_max() won't
  // have NSThread.threadPriority == 1.0. Consider switching to NSThread if a
  // higher priority is required.
  param.sched_priority = static_cast<int>(
      min_priority + relative_priority * (max_priority - min_priority));
  SB_CHECK(pthread_setschedparam(pthread_self(), policy, &param) == 0);

  return true;
}

bool SbThreadGetPriority(SbThreadPriority* priority) {
  struct sched_param param;
  int policy;
  if (pthread_getschedparam(pthread_self(), &policy, &param) != 0) {
    return false;
  }
  int min_priority = sched_get_priority_min(policy);
  int max_priority = sched_get_priority_max(policy);

  return PthreadToSbThreadPriority(param.sched_priority, min_priority,
                                   max_priority, priority);
}
