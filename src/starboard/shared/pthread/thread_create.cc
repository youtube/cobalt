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

#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#include "starboard/log.h"
#include "starboard/shared/pthread/is_success.h"
#include "starboard/string.h"

#if SB_HAS(THREAD_PRIORITY_SUPPORT) && SB_HAS(REAL_TIME_PRIORITY_SUPPORT)
#if !defined(_POSIX_PRIORITY_SCHEDULING)
#error "The _POSIX_PRIORITY_SCHEDULING define indicates that a pthreads \
system supports thread priorities, however this define is not \
defined on this system, contradicting the Starboard configuration \
indicating that priority scheduling is supported."
#endif  // !defined(_POSIX_PRIORITY_SCHEDULING)
#endif  // SB_HAS(THREAD_PRIORITY_SUPPORT) && SB_HAS(REAL_TIME_PRIORITY_SUPPORT)

namespace {

#if SB_HAS(THREAD_PRIORITY_SUPPORT)
#if SB_HAS(REAL_TIME_PRIORITY_SUPPORT)

int SbThreadPriorityToPthread(SbThreadPriority priority) {
  switch (priority) {
    case kSbThreadPriorityLowest:
      SB_NOTREACHED() << "Lowest priority threads should use SCHED_OTHER.";
      return 0;
      break;
    case kSbThreadPriorityLow: return 2;
    case kSbThreadNoPriority:
    // Fall through on purpose to default to kThreadPriority_Normal.
    case kSbThreadPriorityNormal: return 3;
    case kSbThreadPriorityHigh: return 4;
    case kSbThreadPriorityHighest: return 5;
    case kSbThreadPriorityRealTime: return 6;
    default:
      SB_NOTREACHED();
      return 0;
  }
}

#else  // SB_HAS(REAL_TIME_PRIORITY_SUPPORT)

int SbThreadPriorityToNice(SbThreadPriority priority) {
  switch (priority) {
    case kSbThreadPriorityLowest:
      return 10;
    case kSbThreadPriorityLow:
      return 5;
    case kSbThreadNoPriority:
    // Fall through on purpose to default to kThreadPriority_Normal.
    case kSbThreadPriorityNormal:
      return -5;
    case kSbThreadPriorityHigh:
      return -15;
    case kSbThreadPriorityHighest:
      return -18;
    case kSbThreadPriorityRealTime:
      return -19;
    default:
      SB_NOTREACHED();
      return 0;
  }
}

#endif  // #if SB_HAS(REAL_TIME_PRIORITY_SUPPORT)
#endif  // #if SB_HAS(THREAD_PRIORITY_SUPPORT)

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
    SbThreadSetName(thread_params->name);
  }

#if SB_HAS(THREAD_PRIORITY_SUPPORT)
#if SB_HAS(REAL_TIME_PRIORITY_SUPPORT)
  // Use Linux' regular scheduler for lowest priority threads.  Real-time
  // priority threads (of any priority) will always have priority over
  // non-real-time threads (e.g. threads whose scheduler is setup to be
  // SCHED_OTHER, the default scheduler).
  if (thread_params->priority != kSbThreadPriorityLowest) {
    // Note that use of sched_setscheduler() has been found to be more reliably
    // supported than pthread_setschedparam(), so we are using that.
    struct sched_param thread_sched_param;
    thread_sched_param.sched_priority =
        SbThreadPriorityToPthread(thread_params->priority);
    sched_setscheduler(0, SCHED_FIFO, &thread_sched_param);
  }
#else   // #if SB_HAS(REAL_TIME_PRIORITY_SUPPORT)
  // If we don't have real time thread priority support, then set the nice
  // value instead for soft priority support.
  setpriority(PRIO_PROCESS, 0, SbThreadPriorityToNice(thread_params->priority));
#endif  // SB_HAS(REAL_TIME_PRIORITY_SUPPORT)
#endif  // SB_HAS(THREAD_PRIORITY_SUPPORT)

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
    SbStringCopy(params->name, name, SB_ARRAY_SIZE_INT(params->name));
  } else {
    params->name[0] = '\0';
  }

  params->priority = priority;

  SbThread thread = kSbThreadInvalid;
  result = pthread_create(&thread, &attributes, ThreadFunc, params);

  pthread_attr_destroy(&attributes);
  if (IsSuccess(result)) {
    return thread;
  }

  return kSbThreadInvalid;
}
