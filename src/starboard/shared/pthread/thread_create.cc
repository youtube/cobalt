// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <pthread.h>
#include <sched.h>

#include "starboard/shared/pthread/is_success.h"
#include "starboard/string.h"

namespace {

struct ThreadParams {
  SbThreadAffinity affinity;
  SbThreadEntryPoint entry_point;
  char name[128];
  void* context;
};

void* ThreadFunc(void* context) {
  ThreadParams* thread_params = static_cast<ThreadParams*>(context);
  SbThreadEntryPoint entry_point = thread_params->entry_point;
  void* real_context = thread_params->context;
  SbThreadAffinity affinity = thread_params->affinity;
  if (thread_params->name[0] != '\0') {
    SbThreadSetName(thread_params->name);
  }

  delete thread_params;

  if (SbThreadIsValidAffinity(affinity)) {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(affinity, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
  }

  return entry_point(real_context);
}

}  // namespace

SbThread SbThreadCreate(int64_t stack_size,
                        SbThreadPriority priority,
                        SbThreadAffinity affinity,
                        bool joinable,
                        const char* name,
                        SbThreadEntryPoint entry_point,
                        void* context) {
  if (stack_size < 0 || !entry_point) {
    return kSbThreadInvalid;
  }

  pthread_attr_t attributes;
  int result = pthread_attr_init(&attributes);
  if (!IsSuccess(result)) {
    return kSbThreadInvalid;
  }

  pthread_attr_setdetachstate(
      &attributes,
      (joinable ? PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED));
  if (stack_size > 0) {
    pthread_attr_setstacksize(&attributes, stack_size);
  }

  // Here is where we would use priority, but it doesn't really work on Linux
  // without using a realtime scheduling policy, according to this article:
  // http://stackoverflow.com/questions/3649281/how-to-increase-thread-priority-in-pthreads/3663250

  ThreadParams* params = new ThreadParams();
  params->affinity = affinity;
  params->entry_point = entry_point;
  params->context = context;

  if (name) {
    SbStringCopy(params->name, name, SB_ARRAY_SIZE_INT(params->name));
  } else {
    params->name[0] = '\0';
  }

  SbThread thread = kSbThreadInvalid;
  result = pthread_create(&thread, &attributes, ThreadFunc, params);

  pthread_attr_destroy(&attributes);
  if (IsSuccess(result)) {
    return thread;
  }

  return kSbThreadInvalid;
}
