// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: extension of the Starboard Condition Variable module
//
// Implements a convenience class that builds on top of the core Starboard
// condition variable functions.

#ifndef STARBOARD_COMMON_CONDITION_VARIABLE_H_
#define STARBOARD_COMMON_CONDITION_VARIABLE_H_

#ifdef __cplusplus
extern "C++" {
#include <deque>
}  // extern "C++"
#endif

#include "starboard/condition_variable.h"
#include "starboard/mutex.h"
#include "starboard/thread_types.h"
#include "starboard/time.h"
#include "starboard/types.h"

#ifdef __cplusplus
namespace starboard {

// Inline class wrapper for SbConditionVariable.
class ConditionVariable {
 public:
  explicit ConditionVariable(const Mutex& mutex)
      : mutex_(&mutex), condition_() {
    SbConditionVariableCreate(&condition_, mutex_->mutex());
  }

  ~ConditionVariable() { SbConditionVariableDestroy(&condition_); }

  // Releases the mutex and waits for the condition to become true. When this
  // function returns the mutex will have been re-acquired.
  void Wait() const {
    mutex_->debugSetReleased();
    SbConditionVariableWait(&condition_, mutex_->mutex());
    mutex_->debugSetAcquired();
  }

  // Returns |true| if this condition variable was signaled. Otherwise |false|
  // means that the condition variable timed out. In either case the
  // mutex has been re-acquired once this function returns.
  bool WaitTimed(SbTime duration) const {
    mutex_->debugSetReleased();
    bool was_signaled = SbConditionVariableIsSignaled(
        SbConditionVariableWaitTimed(&condition_, mutex_->mutex(), duration));
    mutex_->debugSetAcquired();
    return was_signaled;
  }

  void Broadcast() const { SbConditionVariableBroadcast(&condition_); }

  void Signal() const { SbConditionVariableSignal(&condition_); }

 private:
  const Mutex* mutex_;
  mutable SbConditionVariable condition_;
};

}  // namespace starboard
#endif

#endif  // STARBOARD_COMMON_CONDITION_VARIABLE_H_
