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

#include "starboard/common/condition_variable.h"

#include <sys/time.h>

#include "starboard/common/log.h"
#include "starboard/common/time.h"

namespace starboard {

ConditionVariable::ConditionVariable(const Mutex& mutex)
    : mutex_(&mutex), condition_() {
#if !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
  pthread_condattr_t attribute;
  pthread_condattr_init(&attribute);
  pthread_condattr_setclock(&attribute, CLOCK_MONOTONIC);

  int result = pthread_cond_init(&condition_, &attribute);
  SB_DCHECK(result == 0);

  pthread_condattr_destroy(&attribute);
#else
  int result = pthread_cond_init(&condition_, nullptr);
  SB_DCHECK(result == 0);
#endif  // !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
}

ConditionVariable::~ConditionVariable() {
  pthread_cond_destroy(&condition_);
}

void ConditionVariable::Wait() const {
  mutex_->debugSetReleased();
  pthread_cond_wait(&condition_, mutex_->mutex());
  mutex_->debugSetAcquired();
}

bool ConditionVariable::WaitTimed(int64_t duration) const {
  mutex_->debugSetReleased();
  if (duration < 0) {
    duration = 0;
  }
#if !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
  int64_t timeout_time_usec = starboard::CurrentMonotonicTime();
#else
  int64_t timeout_time_usec = starboard::CurrentPosixTime();
#endif  // !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
  timeout_time_usec += duration;

  // Detect overflow if timeout is near kSbInt64Max. Since timeout can't be
  // negative at this point, if it goes negative after adding now, we know we've
  // gone over. Especially posix now, which has a 400 year advantage over
  // Chromium (Windows) now.
  if (timeout_time_usec < 0) {
    timeout_time_usec = kSbInt64Max;
  }

  struct timespec timeout;
  timeout.tv_sec = timeout_time_usec / 1000'000;
  timeout.tv_nsec = (timeout_time_usec % 1000'000) * 1000;

  bool was_signaled =
      pthread_cond_timedwait(&condition_, mutex_->mutex(), &timeout) == 0;
  mutex_->debugSetAcquired();
  return was_signaled;
}

void ConditionVariable::Broadcast() const {
  pthread_cond_broadcast(&condition_);
}

void ConditionVariable::Signal() const {
  pthread_cond_signal(&condition_);
}

}  // namespace starboard
