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

#if SB_API_VERSION < 16

#include "starboard/condition_variable.h"

#include <pthread.h>

#include "starboard/shared/pthread/is_success.h"
#include "starboard/shared/pthread/types_internal.h"
#include "starboard/shared/starboard/lazy_initialization_internal.h"

using starboard::shared::starboard::EnsureInitialized;

SbConditionVariableResult SbConditionVariableWait(
    SbConditionVariable* condition,
    SbMutex* mutex) {
  if (!condition || !mutex) {
    return kSbConditionVariableFailed;
  }

  if (!EnsureInitialized(
          &(SB_PTHREAD_INTERNAL_CONDITION(condition)->initialized_state))) {
    // The condition variable is set to SB_CONDITION_VARIABLE_INITIALIZER and
    // is uninitialized, so call SbConditionVariableCreate() to initialize the
    // condition variable. SbConditionVariableCreate() is responsible for
    // marking the variable as initialized.
    SbConditionVariableCreate(condition, mutex);
  }

  if (IsSuccess(pthread_cond_wait(
          &(SB_PTHREAD_INTERNAL_CONDITION(condition)->condition),
          SB_PTHREAD_INTERNAL_MUTEX(mutex)))) {
    return kSbConditionVariableSignaled;
  }

  return kSbConditionVariableFailed;
}
#endif  // SB_API_VERSION < 16
