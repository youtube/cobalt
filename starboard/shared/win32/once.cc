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

#include "starboard/once.h"

#include <windows.h>

#include "starboard/common/log.h"
#include "starboard/shared/win32/types_internal.h"

namespace {
BOOL CALLBACK OnceTrampoline(PINIT_ONCE once_control,
                             void* parameter,
                             void** context) {
  static_cast<SbOnceInitRoutine>(parameter)();
  return true;
}

}  // namespace

bool SbOnce(SbOnceControl* once_control, SbOnceInitRoutine init_routine) {
  SB_COMPILE_ASSERT(sizeof(SbOnceControl) >= sizeof(INIT_ONCE),
                    init_once_larger_than_sb_once_control);
  if (!once_control || !init_routine) {
    return false;
  }
  return InitOnceExecuteOnce(SB_WIN32_INTERNAL_ONCE(once_control),
                             OnceTrampoline, init_routine, NULL);
}
