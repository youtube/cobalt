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

#include "starboard/thread_types.h"

namespace starboard {

ConditionVariable::ConditionVariable(const Mutex& mutex)
    : mutex_(&mutex), condition_() {
  SbConditionVariableCreate(&condition_, mutex_->mutex());
}

ConditionVariable::~ConditionVariable() {
  SbConditionVariableDestroy(&condition_);
}

void ConditionVariable::Wait() const {
  mutex_->debugSetReleased();
  SbConditionVariableWait(&condition_, mutex_->mutex());
  mutex_->debugSetAcquired();
}

bool ConditionVariable::WaitTimed(SbTime duration) const {
  mutex_->debugSetReleased();
  bool was_signaled = SbConditionVariableIsSignaled(
      SbConditionVariableWaitTimed(&condition_, mutex_->mutex(), duration));
  mutex_->debugSetAcquired();
  return was_signaled;
}

void ConditionVariable::Broadcast() const {
  SbConditionVariableBroadcast(&condition_);
}

void ConditionVariable::Signal() const {
  SbConditionVariableSignal(&condition_);
}

}  // namespace starboard
