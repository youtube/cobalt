// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/experimental/concurrency_debug.h"

#if SB_ENABLE_CONCURRENCY_DEBUG

#include <algorithm>
#include <string>
#include <vector>

#include "starboard/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/shared/posix/time_internal.h"
#include "starboard/system.h"

namespace starboard {
namespace experimental {
namespace {

const int kMaxSymbolNameLength = 1024;
// Usually there are quite a few contentions at app startup, and it could also
// be unsafe to log as the log system may not be ready.  So ignore the first
// few contentions at app startup set by the following constant.
const int kNumberOfInitialContentionsToIgnore = 50;

const SbTime kMinimumWaitToLog = 5 * kSbTimeMillisecond;
const SbTime kLoggingInterval = 5 * kSbTimeSecond;
const int kStackTraceDepth = 0;

volatile SbAtomic32 s_mutex_acquire_call_counter = 0;
volatile SbAtomic32 s_mutex_acquire_contention_counter = 0;
volatile SbAtomic32 s_mutex_max_contention_time = 0;

}  // namespace

ScopedMutexWaitTracker::ScopedMutexWaitTracker(SbMutex* mutex)
    : acquired_(SbMutexAcquireTry(mutex) == kSbMutexAcquired) {
  SbAtomicNoBarrier_Increment(&s_mutex_acquire_call_counter, 1);
  if (!acquired_) {
    wait_start_ = SbTimeGetMonotonicNow();
  }
}

ScopedMutexWaitTracker::~ScopedMutexWaitTracker() {
  if (kMinimumWaitToLog == 0 || acquired_) {
    return;
  }
  if (SbAtomicNoBarrier_Increment(&s_mutex_acquire_contention_counter, 1) <
      kNumberOfInitialContentionsToIgnore) {
    return;
  }

  auto elapsed = SbTimeGetMonotonicNow() - wait_start_;

  for (;;) {
    SbAtomic32 old_value = s_mutex_max_contention_time;
    if (elapsed <= old_value) {
      break;
    }
    if (SbAtomicNoBarrier_CompareAndSwap(&s_mutex_max_contention_time,
                                         old_value, elapsed) == old_value) {
      break;
    }
  }

  if (elapsed < kMinimumWaitToLog) {
    return;
  }

  SB_LOG(INFO) << "SbMutexAcquire() takes " << elapsed;

  if (kStackTraceDepth > 0) {
    void* stack[kStackTraceDepth];
    int num_stack = SbSystemGetStack(stack, kStackTraceDepth);

    for (int i = 2; i < std::min(num_stack, 5); ++i) {
      char name[kMaxSymbolNameLength + 1];
      if (SbSystemSymbolize(stack[i], name, kMaxSymbolNameLength)) {
        SB_LOG(INFO) << "  - " << name;
      } else {
        SB_LOG(INFO) << "  - 0x" << stack[i];
      }
    }
  }
}

}  // namespace experimental
}  // namespace starboard

#endif  //  SB_ENABLE_CONCURRENCY_DEBUG
