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

#include "starboard/thread.h"

#include <Eina.h>

#include "starboard/log.h"
#include "starboard/shared/pthread/is_success.h"
#include "starboard/string.h"

namespace {

struct ThreadParams {
  SbThreadEntryPoint entry_point;
  char name[128];
  void* context;
};

void* ThreadFunc(void* context, Eina_Thread thread) {
  ThreadParams* thread_params = static_cast<ThreadParams*>(context);
  SbThreadEntryPoint entry_point = thread_params->entry_point;
  void* real_context = thread_params->context;

  if (thread_params->name[0] != '\0') {
    SbThreadSetName(thread_params->name);
  }

  delete thread_params;

  return entry_point(real_context);
}

Eina_Thread_Priority StarboardThreadPriorityToEina(SbThreadPriority priority) {
  switch (priority) {
    case kSbThreadPriorityLowest:
    case kSbThreadPriorityLow:
      return EINA_THREAD_IDLE;
    case kSbThreadNoPriority:
    case kSbThreadPriorityNormal:
      return EINA_THREAD_BACKGROUND;
    case kSbThreadPriorityHigh:
    case kSbThreadPriorityHighest:
    case kSbThreadPriorityRealTime:
      return EINA_THREAD_URGENT;
    default:
      SB_NOTREACHED();
      break;
  }
}

}  // namespace

SbThread SbThreadCreate(int64_t stack_size,
                        SbThreadPriority priority,
                        SbThreadAffinity /*affinity*/,
                        bool joinable,
                        const char* name,
                        SbThreadEntryPoint entry_point,
                        void* context) {
  if (stack_size < 0 || !entry_point) {
    return kSbThreadInvalid;
  }

  ThreadParams* params = new ThreadParams();
  params->entry_point = entry_point;
  params->context = context;
  if (name) {
    SbStringCopy(params->name, name, sizeof(params->name));
  } else {
    params->name[0] = '\0';
  }

  Eina_Thread thread = kSbThreadInvalid;
  if (eina_thread_create(&thread, StarboardThreadPriorityToEina(priority), -1,
                         &ThreadFunc, params)) {
    // Fix nplb SbThreadCreateTest.SunnyDayDetached
    // We can not pass |joinable| info when call eina_thread_create
    // If |joinable| is false, we need to detach thread immediately
    if (!joinable)
      SbThreadDetach(thread);
    return thread;
  }

  return kSbThreadInvalid;
}
