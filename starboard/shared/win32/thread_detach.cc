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

#include "starboard/thread.h"

#include "starboard/condition_variable.h"
#include "starboard/mutex.h"
#include "starboard/shared/win32/thread_private.h"

using starboard::shared::win32::SbThreadPrivate;

void SbThreadDetach(SbThread thread) {
  if (thread == kSbThreadInvalid) {
    return;
  }
  SbThreadPrivate* thread_private = static_cast<SbThreadPrivate*>(thread);

  SbMutexAcquire(&thread_private->mutex_);
  thread_private->wait_for_join_ = false;
  SbConditionVariableSignal(&thread_private->condition_);
  SbMutexRelease(&thread_private->mutex_);
}
