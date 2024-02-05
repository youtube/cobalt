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
  bool result = SbConditionVariableCreate(&condition_, user_mutex_);
  DCHECK(result);
}

ConditionVariable::~ConditionVariable() {
  bool result = SbConditionVariableDestroy(&condition_);
  DCHECK(result);
}

void ConditionVariable::Wait() {
  absl::optional<internal::ScopedBlockingCallWithBaseSyncPrimitives>
      scoped_blocking_call;
  if (waiting_is_blocking_)
    scoped_blocking_call.emplace(FROM_HERE, BlockingType::MAY_BLOCK);

#if DCHECK_IS_ON()
  user_lock_->CheckHeldAndUnmark();
#endif
  SbConditionVariableResult result =
      SbConditionVariableWait(&condition_, user_mutex_);
  DCHECK(SbConditionVariableIsSignaled(result));
#if DCHECK_IS_ON()
  user_lock_->CheckUnheldAndMark();
#endif
}

void ConditionVariable::TimedWait(const TimeDelta& max_time) {
  absl::optional<internal::ScopedBlockingCallWithBaseSyncPrimitives>
      scoped_blocking_call;
  if (waiting_is_blocking_)
    scoped_blocking_call.emplace(FROM_HERE, BlockingType::MAY_BLOCK);
  SbTime duration = max_time.ToSbTime();

#if DCHECK_IS_ON()
  user_lock_->CheckHeldAndUnmark();
#endif
  SbConditionVariableResult result =
      SbConditionVariableWaitTimed(&condition_, user_mutex_, duration);
  DCHECK_NE(kSbConditionVariableFailed, result);
#if DCHECK_IS_ON()
  user_lock_->CheckUnheldAndMark();
#endif
}

void ConditionVariable::Broadcast() {
  bool result = SbConditionVariableBroadcast(&condition_);
  DCHECK(result);
}

void ConditionVariable::Signal() {
  bool result = SbConditionVariableSignal(&condition_);
  DCHECK(result);
}

}  // namespace base
