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

bool SbOnce(SbOnceControl* once_control, SbOnceInitRoutine init_routine) {
  SB_COMPILE_ASSERT(sizeof(SbOnceControl) >= sizeof(pthread_once_t),
                    pthread_once_t_larger_than_sb_once_control);
  if (once_control == NULL) {
    return false;
  }
  if (init_routine == NULL) {
    return false;
  }

  return pthread_once(SB_PTHREAD_INTERNAL_ONCE(once_control), init_routine) ==
         0;
}
