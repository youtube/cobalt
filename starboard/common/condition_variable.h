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

// TODO: b/390503926 - Remove the starboard condition variable API and all it's
// references. See work done in 25lts as an example.

#include <pthread.h>

#include "starboard/common/mutex.h"
#include "starboard/configuration.h"
#include "starboard/types.h"

#ifdef __cplusplus

extern "C++" {

namespace starboard {

// Inline class wrapper for pthread_cond_t.
class ConditionVariable {
 public:
  explicit ConditionVariable(const Mutex& mutex);
  ~ConditionVariable();

  // Releases the mutex and waits for the condition to become true. When this
  // function returns the mutex will have been re-acquired.
  void Wait() const;

  // Returns |true| if this condition variable was signaled. Otherwise |false|
  // means that the condition variable timed out. In either case the
  // mutex has been re-acquired once this function returns. The |duration| is
  // microseconds.
  bool WaitTimed(int64_t duration) const;

  void Broadcast() const;
  void Signal() const;

 private:
  const Mutex* mutex_;
  mutable pthread_cond_t condition_;
};

}  // namespace starboard
}

#endif  //__cplusplus

#endif  // STARBOARD_COMMON_CONDITION_VARIABLE_H_
