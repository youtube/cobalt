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

#include "starboard/common/semaphore.h"

namespace starboard {

Semaphore::Semaphore() : mutex_(), condition_(mutex_), permits_(0) {}

Semaphore::Semaphore(int initial_thread_permits)
    : mutex_(), condition_(mutex_), permits_(initial_thread_permits) {}

Semaphore::~Semaphore() {}

void Semaphore::Put() {
  ScopedLock lock(mutex_);
  ++permits_;
  condition_.Signal();
}

void Semaphore::Take() {
  ScopedLock lock(mutex_);
  while (permits_ <= 0) {
    condition_.Wait();
  }
  --permits_;
}

bool Semaphore::TakeTry() {
  ScopedLock lock(mutex_);
  if (permits_ <= 0) {
    return false;
  }
  --permits_;
  return true;
}

bool Semaphore::TakeWait(SbTime wait_us) {
  if (wait_us <= 0) {
    return TakeTry();
  }
  SbTime expire_time = SbTimeGetMonotonicNow() + wait_us;
  ScopedLock lock(mutex_);
  while (permits_ <= 0) {
    SbTime remaining_wait_time = expire_time - SbTimeGetMonotonicNow();
    if (remaining_wait_time <= 0) {
      return false;  // Timed out.
    }
    bool was_signaled = condition_.WaitTimed(remaining_wait_time);
    if (!was_signaled) {
      return false;  // Timed out.
    }
  }
  --permits_;
  return true;
}

}  // namespace starboard
