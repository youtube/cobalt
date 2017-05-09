// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/condition_variable.h"

#include <windows.h>

#include "starboard/shared/win32/time_utils.h"

using starboard::shared::win32::ConvertSbTimeToMillisRoundUp;

SbConditionVariableResult SbConditionVariableWaitTimed(
    SbConditionVariable* condition,
    SbMutex* mutex,
    SbTime timeout) {
  if (!condition || !mutex) {
    return kSbConditionVariableFailed;
  }

  if (timeout < 0) {
    timeout = 0;
  }
  bool result = SleepConditionVariableSRW(
      reinterpret_cast<PCONDITION_VARIABLE>(condition),
      reinterpret_cast<PSRWLOCK>(mutex),
      ConvertSbTimeToMillisRoundUp(timeout), 0);

  if (timeout == 0) {
    // Per documentation, "If the |timeout_duration| value is less than
    // or equal to zero, the function returns as quickly as possible with a
    // kSbConditionVariableTimedOut result."
    return kSbConditionVariableTimedOut;
  }

  if (result) {
    return kSbConditionVariableSignaled;
  }

  if (GetLastError() == ERROR_TIMEOUT) {
    return kSbConditionVariableTimedOut;
  }
  return kSbConditionVariableFailed;
}
