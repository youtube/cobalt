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

#ifndef STARBOARD_COMMON_SEMAPHORE_H_
#define STARBOARD_COMMON_SEMAPHORE_H_

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/configuration.h"
#include "starboard/time.h"

namespace starboard {

// A cross platform semaphore implementation.
class Semaphore {
 public:
  Semaphore();  // initial_thread_permits = 0;
  explicit Semaphore(int initial_thread_permits);
  ~Semaphore();

  // Increases the permits. One thread will be woken up if it is blocked in
  // Take().
  void Put();

  // The caller is blocked if the counter is negative, and will stay blocked
  // until Put() is invoked by another thread. When Take() is/becomes unblocked
  // then the permits is then decremented by one.
  void Take();

  // A non-blocking version of Take(). If the counter is negative then this
  // function returns immediately and the semaphore is not modified. If true
  // is returned then the effects are the same as if Take() had been invoked.
  bool TakeTry();

  // Same as Take(), but will wait at most, wait_us microseconds.
  // Returns |false| if the semaphore timed out, |true| otherwise.
  bool TakeWait(SbTime wait_us);

 private:
  Mutex mutex_;
  ConditionVariable condition_;
  int permits_;

  Semaphore(const Semaphore&) = delete;
  void operator=(const Semaphore&) = delete;
};

}  // namespace starboard.

#endif  // STARBOARD_COMMON_SEMAPHORE_H_
