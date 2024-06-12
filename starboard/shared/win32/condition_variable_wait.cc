// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/condition_variable.h"

#include <windows.h>

#include "starboard/shared/win32/types_internal.h"

SbConditionVariableResult SbConditionVariableWait(
    SbConditionVariable* condition,
    SbMutex* mutex) {
  if (!condition || !mutex) {
    return kSbConditionVariableFailed;
  }
  bool result =
      SleepConditionVariableSRW(SB_WIN32_INTERNAL_CONDITION(condition),
                                SB_WIN32_INTERNAL_MUTEX(mutex), INFINITE, 0);

  return result ? kSbConditionVariableSignaled : kSbConditionVariableFailed;
}

#endif  // SB_API_VERSION < 16
