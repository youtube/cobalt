// Copyright 2016 Google Inc. All Rights Reserved.
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

// Defines a set of atomic integer operations that can be used as lightweight
// synchronization or as building blocks for heavier synchronization
// primitives. Their use is very subtle and requires detailed understanding of
// the behavior of supported architectures, so their direct use is not
// recommended except when rigorously deemed absolutely necessary for
// performance reasons.

// This file contains Starboard implementations for synchronization primiatives
// used in ICU.

#include "third_party/icu/source/starboard/mutex_sb.h"

#include "starboard/atomic.h"
#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "third_party/icu/source/common/uassert.h"
#include "third_party/icu/source/common/umutex.h"
#include "third_party/icu/source/common/unicode/utypes.h"

U_CAPI void U_EXPORT2 umtx_lock(UMutex* mutex) {
  if (mutex == NULL) {
    // When ICU implementation pass NULL for mutex, the intended behavior
    // is to use the globalMutex.  See umutex.cpp for details.
    mutex = &globalMutex;
  }
  SbMutexAcquire(mutex);
}

U_CAPI void U_EXPORT2 umtx_unlock(UMutex* mutex) {
  if (mutex == NULL) {
    // When ICU implementation pass NULL for mutex, the intended behavior
    // is to use the globalMutex.  See umutex.cpp for details.
    mutex = &globalMutex;
  }
  SbMutexRelease(mutex);
}

U_CAPI void U_EXPORT2 umtx_condSignal(UConditionVar* condition) {
  bool is_success = SbConditionVariableSignal(condition);
  U_ASSERT(is_success);
}

U_CAPI void U_EXPORT2 umtx_condBroadcast(UConditionVar* condition) {
  bool is_success = SbConditionVariableBroadcast(condition);
  U_ASSERT(is_success);
}

U_CAPI void U_EXPORT2 umtx_condWait(UConditionVar* condition, UMutex* mutex) {
  SbConditionVariableResult result = SbConditionVariableWait(condition, mutex);
  U_ASSERT(result == kSbConditionVariableSignaled);
}

static SbMutex init_mutex = SB_MUTEX_INITIALIZER;
static SbConditionVariable init_condition_variable =
    SB_CONDITION_VARIABLE_INITIALIZER;

U_NAMESPACE_BEGIN

U_COMMON_API UBool U_EXPORT2 umtx_initImplPreInit(UInitOnce& uio) {
  umtx_lock(&init_mutex);

  bool return_value = FALSE;
  int32_t state = uio.fState;
  if (state == 0) {
    // This thread has won the race.
    // Update uio.fState so that other threads can find out.
    umtx_storeRelease(uio.fState, 1);
    return_value = TRUE;
  } else {
    // The state can now be 1, or 2.
    while (uio.fState == 1) {  // This will be changed by ImplPostInit.
      umtx_condWait(&init_condition_variable, &init_mutex);
    }
    U_ASSERT(uio.fState == 2);
  }

  umtx_unlock(&init_mutex);
  return return_value;
}

U_COMMON_API void U_EXPORT2 umtx_initImplPostInit(UInitOnce& uio) {
  umtx_lock(&init_mutex);
  U_ASSERT(uio.fState == 1);
  umtx_storeRelease(uio.fState, 2);
  umtx_condBroadcast(&init_condition_variable);
  umtx_unlock(&init_mutex);
}

U_NAMESPACE_END
