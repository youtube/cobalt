// Copyright 2015 Google Inc. All Rights Reserved.
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

// Condition variables.

#ifndef STARBOARD_CONDITION_VARIABLE_H_
#define STARBOARD_CONDITION_VARIABLE_H_

#ifdef __cplusplus
#include <deque>
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
// placing the newly created condition variable in |out_condition|. Returns
// whether the condition variable could be created.
// TODO: It looks like WTF does not have the mutex available when creating
// the condition variable, and pthreads doesn't appear to require the mutex on
// condvar creation, so we should just remove the parameter.
SB_EXPORT bool SbConditionVariableCreate(SbConditionVariable* out_condition,
                                         SbMutex* opt_mutex);

// Destroys a condition variable, returning whether the destruction was
// successful. The condition variable specified by |condition| is
// invalidated. Behavior is undefined if other threads are currently waiting
// on this condition variable.
SB_EXPORT bool SbConditionVariableDestroy(SbConditionVariable* condition);

// Waits for |condition|, releasing the held lock |mutex|, blocking
// indefinitely, and returning the result. Behavior is undefined if |mutex| is
// not held.
SB_EXPORT SbConditionVariableResult
SbConditionVariableWait(SbConditionVariable* condition, SbMutex* mutex);

// Waits for |condition|, releasing the held lock |mutex|, blocking up to
// |timeout_duration|, and returning the acquisition result. If
// |timeout_duration| is less than or equal to zero, it will return as quickly
// as possible with a kSbConditionVariableTimedOut result. Behavior is undefined
// if |mutex| is not held.
SB_EXPORT SbConditionVariableResult
SbConditionVariableWaitTimed(SbConditionVariable* condition,
                             SbMutex* mutex,
                             SbTime timeout_duration);

// Broadcasts to all current waiters of |condition| to stop waiting.
SB_EXPORT bool SbConditionVariableBroadcast(SbConditionVariable* condition);

// Signals the next waiter of |condition| to stop waiting.
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
      : mutex_(mutex.mutex()), condition_() {
    SbConditionVariableCreate(&condition_, mutex_);
  }

  ~ConditionVariable() { SbConditionVariableDestroy(&condition_); }

  void Wait() const { SbConditionVariableWait(&condition_, mutex_); }

  bool WaitTimed(SbTime duration) const {
    return SbConditionVariableIsSignaled(
        SbConditionVariableWaitTimed(&condition_, mutex_, duration));
  }

  void Broadcast() const { SbConditionVariableBroadcast(&condition_); }

  void Signal() const { SbConditionVariableSignal(&condition_); }

 private:
  mutable SbMutex* mutex_;
  mutable SbConditionVariable condition_;
};

}  // namespace starboard
#endif

#endif  // STARBOARD_CONDITION_VARIABLE_H_
