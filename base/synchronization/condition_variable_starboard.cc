// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "base/synchronization/condition_variable.h"

#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"

namespace base {

ConditionVariable::ConditionVariable(Lock* user_lock)
    : user_mutex_(user_lock->lock_.native_handle())
#if DCHECK_IS_ON()
      ,
      user_lock_(user_lock)
#endif
{
#if !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
  pthread_condattr_t attribute;
  pthread_condattr_init(&attribute);
  pthread_condattr_setclock(&attribute, CLOCK_MONOTONIC);

  int result = pthread_cond_init(&condition_, &attribute);
  DCHECK(result == 0);

  pthread_condattr_destroy(&attribute);
#else
  int result = pthread_cond_init(&condition_, nullptr);
  DCHECK(result == 0);
#endif  // !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
}

ConditionVariable::~ConditionVariable() {
  int result = pthread_cond_destroy(&condition_);
  DCHECK(result == 0);
}

void ConditionVariable::Wait() {
  absl::optional<internal::ScopedBlockingCallWithBaseSyncPrimitives>
      scoped_blocking_call;
  if (waiting_is_blocking_)
    scoped_blocking_call.emplace(FROM_HERE, BlockingType::MAY_BLOCK);

#if DCHECK_IS_ON()
  user_lock_->CheckHeldAndUnmark();
#endif
  int result = pthread_cond_wait(&condition_, user_mutex_);
  DCHECK(result == 0);
#if DCHECK_IS_ON()
  user_lock_->CheckUnheldAndMark();
#endif
}

void ConditionVariable::TimedWait(const TimeDelta& max_time) {
  absl::optional<internal::ScopedBlockingCallWithBaseSyncPrimitives>
      scoped_blocking_call;
  if (waiting_is_blocking_)
    scoped_blocking_call.emplace(FROM_HERE, BlockingType::MAY_BLOCK);
  int64_t duration = max_time.InMicroseconds();

#if DCHECK_IS_ON()
  user_lock_->CheckHeldAndUnmark();
#endif
#if !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
  int64_t timeout_time_usec = starboard::CurrentMonotonicTime();
#else
  int64_t timeout_time_usec = starboard::CurrentPosixTime();
#endif  // !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
  timeout_time_usec += max_time.InMicroseconds();

  struct timespec timeout;
  timeout.tv_sec = timeout_time_usec / 1000'000;
  timeout.tv_nsec = (timeout_time_usec % 1000'000) * 1000;

  int result = pthread_cond_timedwait(&condition_, user_mutex_, &timeout);
  DCHECK(result == 0 || result == ETIMEDOUT);
#if DCHECK_IS_ON()
  user_lock_->CheckUnheldAndMark();
#endif
}

void ConditionVariable::Broadcast() {
  int result = pthread_cond_broadcast(&condition_);
  DCHECK(result == 0);
}

void ConditionVariable::Signal() {
  int result = pthread_cond_signal(&condition_);
  DCHECK(result == 0);
}

}  // namespace base
