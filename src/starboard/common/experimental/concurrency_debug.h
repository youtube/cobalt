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

#ifndef STARBOARD_COMMON_EXPERIMENTAL_CONCURRENCY_DEBUG_H_
#define STARBOARD_COMMON_EXPERIMENTAL_CONCURRENCY_DEBUG_H_

#define SB_ENABLE_CONCURRENCY_DEBUG 0

#if SB_ENABLE_CONCURRENCY_DEBUG

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/mutex.h"
#include "starboard/thread.h"
#include "starboard/time.h"

// WARNING: Features inside experimental namespace is strictly experimental and
// can be changed or removed at any time, even within the same Cobalt/Starboard
// version.
namespace starboard {
namespace experimental {

// The class can be used to track any mutex acquire that takes extensive time.
// It can be used by adding the following block of code into SbMutexAcquire()
// implementation, right before the system call to acquire the mutex:
//   starboard::experimental::ScopedMutexWaitTracker tracker(mutex);
//   if (tracker.acquired()) {
//     return kSbMutexAcquired;
//   }
class ScopedMutexWaitTracker {
 public:
  explicit ScopedMutexWaitTracker(SbMutex* mutex);
  ~ScopedMutexWaitTracker();

  bool acquired() const { return acquired_; }

 private:
  const bool acquired_;
  SbTime wait_start_;
};

// The class can be used to track how much CPU the thread consumes periodically.
// It can be used by declaring "ThreadTracker s_thread_tracker;" as a static
// object, and then call "s_thread_tracker.CheckPoint(interval);" periodically
// on the thread being tracked.  It will print out the CPU time consumed by the
// thread at interval specified by |interval|.
class ThreadTracker {
 public:
  void CheckPoint(SbTime interval) {
    SbTime wallclock_now = SbTimeGetMonotonicNow();

    if (wallclock_now - last_wallclock_time_ < interval) {
      return;
    }

    char name[kSbMaxThreadNameLength + 1];
    SbThreadGetName(name, kSbMaxThreadNameLength);

    SbTime thread_now = SbTimeGetMonotonicThreadNow();
    SB_LOG(INFO) << "Thread " << name << " uses "
                 << thread_now - last_thread_time_ << " during "
                 << wallclock_now - last_wallclock_time_;
    last_wallclock_time_ = wallclock_now;
    last_thread_time_ = thread_now;
  }

 private:
  SbTime last_wallclock_time_ = SbTimeGetMonotonicNow();
  SbTime last_thread_time_ = SbTimeGetMonotonicThreadNow();
};

}  // namespace experimental
}  // namespace starboard

#endif  // SB_ENABLE_CONCURRENCY_DEBUG

#endif  // STARBOARD_COMMON_EXPERIMENTAL_CONCURRENCY_DEBUG_H_
