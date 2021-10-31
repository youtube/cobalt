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

#include "starboard/once.h"

#include <pthread.h>

#include "starboard/common/log.h"
#include "starboard/shared/pthread/types_internal.h"
#include "starboard/shared/starboard/lazy_initialization_internal.h"

using starboard::shared::starboard::EnsureInitialized;
using starboard::shared::starboard::SetInitialized;

bool SbOnce(SbOnceControl* once_control, SbOnceInitRoutine init_routine) {
#if SB_API_VERSION >= 12
  SB_COMPILE_ASSERT(sizeof(SbOnceControl) >= sizeof(SbOnceControlPrivate),
                    sb_once_control_private_larger_than_sb_once_control);
#endif
  if (once_control == NULL) {
    return false;
  }
  if (init_routine == NULL) {
    return false;
  }
#if SB_API_VERSION >= 12
  if (!EnsureInitialized(
          &(SB_INTERNAL_ONCE(once_control)->initialized_state))) {
    *SB_PTHREAD_INTERNAL_ONCE(once_control) = PTHREAD_ONCE_INIT;
    SetInitialized(&(SB_INTERNAL_ONCE(once_control)->initialized_state));
  }
#endif

  return pthread_once(SB_PTHREAD_INTERNAL_ONCE(once_control), init_routine) ==
         0;
}
