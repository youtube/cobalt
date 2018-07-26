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

// Module Overview: Starboard Condition Variable module
//
// Defines an interface for condition variables.

#ifndef STARBOARD_CONDITION_VARIABLE_H_
#define STARBOARD_CONDITION_VARIABLE_H_

#ifdef __cplusplus
extern "C++" {
#include <deque>
}  // extern "C++"
#endif

#include "starboard/export.h"
#include "starboard/mutex.h"
#include "starboard/thread_types.h"
#include "starboard/time.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Enumeration of possible results from waiting on a condvar.
typedef enum SbConditionVariableResult {
  // The wait completed because the condition variable was signaled.
  kSbConditionVariableSignaled,

  // The wait completed because it timed out, and was not signaled.
  kSbConditionVariableTimedOut,

  // The wait failed, either because a parameter wasn't valid, or the condition
  // variable has already been destroyed, or something similar.
  kSbConditionVariableFailed,
} SbConditionVariableResult;

// Returns whether the given result is a success.
static SB_C_INLINE bool SbConditionVariableIsSignaled(
    SbConditionVariableResult result) {
  return result == kSbConditionVariableSignaled;
}

// Creates a new condition variable to work with |opt_mutex|, which may be null,
// placing the newly created condition variable in |out_condition|.
//
// The return value indicates whether the condition variable could be created.
SB_EXPORT bool SbConditionVariableCreate(SbConditionVariable* out_condition,
                                         SbMutex* opt_mutex);

// Destroys the specified SbConditionVariable. The return value indicates
// whether the destruction was successful. The behavior is undefined if other
// threads are currently waiting on this condition variable.
//
// |condition|: The SbConditionVariable to be destroyed. This invalidates the
// condition variable.
SB_EXPORT bool SbConditionVariableDestroy(SbConditionVariable* condition);

// Waits for |condition|, releasing the held lock |mutex|, blocking
// indefinitely, and returning the result. Behavior is undefined if |mutex| is
// not held.
SB_EXPORT SbConditionVariableResult
SbConditionVariableWait(SbConditionVariable* condition, SbMutex* mutex);

// Waits for |condition|, releasing the held lock |mutex|, blocking up to
// |timeout_duration|, and returning the acquisition result. Behavior is
// undefined if |mutex| is not held.
//
// |timeout_duration|: The maximum amount of time that function should wait
// for |condition|. If the |timeout_duration| value is less than or equal to
// zero, the function returns as quickly as possible with a
// kSbConditionVariableTimedOut result.
SB_EXPORT SbConditionVariableResult
SbConditionVariableWaitTimed(SbConditionVariable* condition,
                             SbMutex* mutex,
                             SbTime timeout_duration);

// Broadcasts to all current waiters of |condition| to stop waiting. This
// function wakes all of the threads waiting on |condition| while
// SbConditionVariableSignal wakes a single thread.
//
// |condition|: The condition that should no longer be waited for.
SB_EXPORT bool SbConditionVariableBroadcast(SbConditionVariable* condition);

// Signals the next waiter of |condition| to stop waiting. This function wakes
// a single thread waiting on |condition| while SbConditionVariableBroadcast
// wakes all threads waiting on it.
//
// |condition|: The condition that the waiter should stop waiting for.
SB_EXPORT bool SbConditionVariableSignal(SbConditionVariable* condition);

#ifdef __cplusplus
}  // extern "C"
#endif

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

#endif  // STARBOARD_CONDITION_VARIABLE_H_
