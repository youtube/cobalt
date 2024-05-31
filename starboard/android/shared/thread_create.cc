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

#include "starboard/thread.h"

#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/shared/pthread/is_success.h"
#include "starboard/shared/pthread/thread_create_priority.h"
#include "starboard/shared/pthread/types_internal.h"

namespace starboard {
namespace shared {
namespace pthread {

void PreThreadRun() {}

void PostThreadRun() {
  // TODO: Currently OnThreadFinish() is specific to the android-forked version
  // of this file, thread_create.cc.  This functionality could be implemented
  // though similar to ThreadSetPriority(), such that we allow platforms to
  // override the OnThreadStart()/OnThreadFinish() logic, if they want, and then
  // we don't have to fork this file.  The only reason that is not done
  // currently is because of bad timing with respect to Starboard interface
  // changes.
  android::shared::JniEnvExt::OnThreadShutdown();
}

}  // namespace pthread
}  // namespace shared
}  // namespace starboard

namespace {

struct ThreadParams {
  SbThreadAffinity affinity;
  SbThreadEntryPoint entry_point;
  char name[128];
  void* context;
  SbThreadPriority priority;
};

void* ThreadFunc(void* context) {
  ThreadParams* thread_params = static_cast<ThreadParams*>(context);
  SbThreadEntryPoint entry_point = thread_params->entry_point;
  void* real_context = thread_params->context;
  SbThreadAffinity affinity = thread_params->affinity;
  if (thread_params->name[0] != '\0') {
    pthread_setname_np(pthread_self(), thread_params->name);
  }

  starboard::shared::pthread::ThreadSetPriority(thread_params->priority);

  delete thread_params;

  if (SbThreadIsValidAffinity(affinity)) {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(affinity, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
  }

  starboard::shared::pthread::PreThreadRun();

  void* return_value = entry_point(real_context);

  starboard::shared::pthread::PostThreadRun();

  return return_value;
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

#if defined(ADDRESS_SANITIZER)
  // Set a big thread stack size when in ADDRESS_SANITIZER mode.
  // This eliminates buffer overflows for deeply nested callstacks.
  if (stack_size == 0) {
    stack_size = 4096 * 1024;  // 4MB
  }
#endif

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

  ThreadParams* params = new ThreadParams();
  params->affinity = affinity;
  params->entry_point = entry_point;
  params->context = context;

  if (name) {
    starboard::strlcpy(params->name, name, SB_ARRAY_SIZE_INT(params->name));
  } else {
    params->name[0] = '\0';
  }

  params->priority = priority;

  SbThread thread = kSbThreadInvalid;
  SB_COMPILE_ASSERT(sizeof(SbThread) >= sizeof(pthread_t),
                    pthread_t_larger_than_sb_thread);
  result = pthread_create(SB_PTHREAD_INTERNAL_THREAD_PTR(thread), &attributes,
                          ThreadFunc, params);

  pthread_attr_destroy(&attributes);
  if (IsSuccess(result)) {
    return thread;
  }

  return kSbThreadInvalid;
}
#endif  // SB_API_VERSION < 16
