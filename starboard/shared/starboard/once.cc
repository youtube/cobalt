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
#include "starboard/shared/starboard/lazy_initialization_internal.h"

// Platform-independent SbOnce() implementation based on the functionality
// declared by starboard/shared/lazy_initialization_internal.h which internally
// uses atomics.
using starboard::shared::starboard::EnsureInitialized;
using starboard::shared::starboard::SetInitialized;

bool SbOnce(SbOnceControl* once_control, SbOnceInitRoutine init_routine) {
  if (once_control == NULL) {
    return false;
  }
  if (init_routine == NULL) {
    return false;
  }

  if (!EnsureInitialized(once_control)) {
    init_routine();
    SetInitialized(once_control);
  }

  return true;
}
