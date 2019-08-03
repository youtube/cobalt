/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/synchronization/condition_variable.h"

#include <errno.h>

#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/time.h"

namespace base {

ConditionVariable::ConditionVariable(Lock* user_lock)
    : user_mutex_(user_lock->lock_.os_lock())
#if !defined(NDEBUG)
    , user_lock_(user_lock)
#endif
{
  int rv = lb_shell_cond_init(&condition_, user_mutex_);
  DCHECK_EQ(0, rv);
}

ConditionVariable::~ConditionVariable() {
  int rv = lb_shell_cond_destroy(&condition_);
  DCHECK_EQ(0, rv);
}

void ConditionVariable::Wait() {
#if !defined(NDEBUG)
  user_lock_->CheckHeldAndUnmark();
#endif
  int rv = lb_shell_cond_wait(&condition_, user_mutex_);
  DCHECK_EQ(0, rv);
#if !defined(NDEBUG)
  user_lock_->CheckUnheldAndMark();
#endif
}

void ConditionVariable::TimedWait(const TimeDelta& max_time) {
  int64 usecs = max_time.InMicroseconds();

#if !defined(NDEBUG)
  user_lock_->CheckHeldAndUnmark();
#endif
  int rv = lb_shell_cond_timedwait(&condition_, user_mutex_, usecs);
  DCHECK(rv == 0 || rv == ETIMEDOUT);
#if !defined(NDEBUG)
  user_lock_->CheckUnheldAndMark();
#endif
}

void ConditionVariable::Broadcast() {
  int rv = lb_shell_cond_broadcast(&condition_);
  DCHECK_EQ(0, rv);
}

void ConditionVariable::Signal() {
  int rv = lb_shell_cond_signal(&condition_);
  DCHECK_EQ(0, rv);
}

}  // namespace base
