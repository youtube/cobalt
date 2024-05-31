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

using starboard::shared::starboard::IsInitialized;

bool SbConditionVariableBroadcast(SbConditionVariable* condition) {
  if (!condition) {
    return false;
  }

  if (!IsInitialized(
          &(SB_PTHREAD_INTERNAL_CONDITION(condition)->initialized_state))) {
    // If the condition variable is not initialized yet, then there is nothing
    // to signal so vacuously return true.
    return true;
  }

  return IsSuccess(pthread_cond_broadcast(
      &(SB_PTHREAD_INTERNAL_CONDITION(condition)->condition)));
}

#endif  // SB_API_VERSION < 16
