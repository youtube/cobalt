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

#include "starboard/thread.h"

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/shared/win32/thread_private.h"

using starboard::shared::win32::SbThreadPrivate;

bool SbThreadJoin(SbThread thread, void** out_return) {
  if (thread == kSbThreadInvalid) {
    return false;
  }

  SbThreadPrivate* thread_private = static_cast<SbThreadPrivate*>(thread);

  SbMutexAcquire(&thread_private->mutex_);
  if (!thread_private->wait_for_join_) {
    // Thread has already been detached.
    SbMutexRelease(&thread_private->mutex_);
    return false;
  }
  while (!thread_private->result_is_valid_) {
    SbConditionVariableWait(&thread_private->condition_,
                            &thread_private->mutex_);
  }
  thread_private->wait_for_join_ = false;
  SbConditionVariableSignal(&thread_private->condition_);
  if (out_return != NULL) {
    *out_return = thread_private->result_;
  }
  SbMutexRelease(&thread_private->mutex_);
  return true;
}
