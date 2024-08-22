// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "base/threading/platform_thread.h"

#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_restrictions.h"
#include "starboard/configuration_constants.h"
#include "starboard/thread.h"

namespace base {

namespace {

struct ThreadParams {
  PlatformThread::Delegate* delegate;
  bool joinable;
  SbThreadPriority thread_priority;
  std::string thread_name;
};

void* ThreadFunc(void* params) {
  ThreadParams* thread_params = static_cast<ThreadParams*>(params);
  PlatformThread::Delegate* delegate = thread_params->delegate;

#if SB_API_VERSION >= 16
  if (kSbHasThreadPrioritySupport) {
    SbThreadSetPriority(thread_params->thread_priority);
  }
#endif  // SB_API_VERSION >= 16
  pthread_setname_np(pthread_self(), thread_params->thread_name.c_str());

  absl::optional<ScopedDisallowSingleton> disallow_singleton;
  if (!thread_params->joinable) {
    disallow_singleton.emplace();
  }

  delete thread_params;

  ThreadIdNameManager::GetInstance()->RegisterThread(
      PlatformThread::CurrentHandle().platform_handle(),
      PlatformThread::CurrentId());

  delegate->ThreadMain();

  ThreadIdNameManager::GetInstance()->RemoveName(
      PlatformThread::CurrentHandle().platform_handle(),
      PlatformThread::CurrentId());

  return NULL;
}

#if SB_API_VERSION >= 16
bool CreateThread(size_t stack_size,
                  SbThreadPriority priority,
                  bool joinable,
                  const char* name,
                  PlatformThread::Delegate* delegate,
                  PlatformThreadHandle* thread_handle) {
  ThreadParams* params = new ThreadParams;
  params->delegate = delegate;
  params->joinable = joinable;
  params->thread_priority = priority;
  if (name != nullptr) {
    params->thread_name = name;
  }

  pthread_attr_t attr;
  if (pthread_attr_init(&attr) != 0) {
    return false;
  }

  pthread_attr_setstacksize(&attr, stack_size);

  if (!joinable) {
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  }

  pthread_t thread = 0;
  pthread_create(&thread, &attr, ThreadFunc, params);
  pthread_attr_destroy(&attr);

  if (thread != 0) {
    if (thread_handle) {
      *thread_handle = PlatformThreadHandle(thread);
    }

    return true;
  }

  return false;
}
#else
bool CreateThread(size_t stack_size,
                  SbThreadPriority priority,
                  bool joinable,
                  const char* name,
                  PlatformThread::Delegate* delegate,
                  PlatformThreadHandle* thread_handle) {
  ThreadParams* params = new ThreadParams;
  params->delegate = delegate;
  params->joinable = joinable;

  SbThread thread = SbThreadCreate(stack_size, priority, kSbThreadNoAffinity, joinable,
                                   name, ThreadFunc, params);
  if (SbThreadIsValid(thread)) {
    if (thread_handle) {
      *thread_handle = PlatformThreadHandle(thread);
    }

    return true;
  }

  return false;
}
#endif  // SB_API_VERSION >= 16

inline SbThreadPriority toSbPriority(ThreadType priority) {
  switch (priority) {
    case ThreadType::kBackground:
      return kSbThreadPriorityLowest;
    case ThreadType::kUtility:
      return kSbThreadPriorityLow;
    case ThreadType::kResourceEfficient:
      return kSbThreadPriorityNormal;
    case ThreadType::kDefault:
      return kSbThreadNoPriority;
    case ThreadType::kCompositing:
      return kSbThreadPriorityHigh;
    case ThreadType::kDisplayCritical:
      return kSbThreadPriorityHighest;
    case ThreadType::kRealtimeAudio:
      return kSbThreadPriorityRealTime;
  };
  NOTREACHED();
}
}  // namespace

// static
PlatformThreadId PlatformThread::CurrentId() {
  return SbThreadGetId();
}

// static
PlatformThreadRef PlatformThread::CurrentRef() {
  return PlatformThreadRef(pthread_self());

}

// static
PlatformThreadHandle PlatformThread::CurrentHandle() {
  return PlatformThreadHandle(pthread_self());
}

// static
void PlatformThread::YieldCurrentThread() {
  sched_yield();
}

// static
void PlatformThread::Sleep(TimeDelta duration) {
  usleep(duration.InMicroseconds());
}

// static
void PlatformThread::SetName(const std::string& name) {
  ThreadIdNameManager::GetInstance()->SetName(name);

  std::string buffer(name, 0, kSbMaxThreadNameLength - 1);
  pthread_setname_np(pthread_self(), buffer.c_str());
}

// static
const char* PlatformThread::GetName() {
  return ThreadIdNameManager::GetInstance()->GetName(CurrentId());
}

// static
bool PlatformThread::CreateWithType(size_t stack_size,
                                        Delegate* delegate,
                                        PlatformThreadHandle* thread_handle,
                                        ThreadType priority,
                                        MessagePumpType /* pump_type_hint */) {
  return CreateThread(stack_size, toSbPriority(priority),
                      true /* joinable thread */, NULL, delegate,
                      thread_handle);
}

// static
bool PlatformThread::CreateNonJoinableWithType(size_t stack_size,
                                                   Delegate* delegate,
                                                   ThreadType priority,
                                                   MessagePumpType /* pump_type_hint */) {
  return CreateThread(stack_size, toSbPriority(priority),
                      false /* joinable thread */, NULL, delegate, NULL);
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  // Joining another thread may block the current thread for a long time, since
  // the thread referred to by |thread_handle| may still be running long-lived /
  // blocking tasks.
  internal::AssertBlockingAllowed();
  pthread_join(thread_handle.platform_handle(), NULL);
}

void PlatformThread::Detach(PlatformThreadHandle thread_handle) {
  pthread_detach(thread_handle.platform_handle());
}

void internal::SetCurrentThreadTypeImpl(ThreadType /* thread_type */, MessagePumpType /*pump_type_hint*/) {
  NOTIMPLEMENTED();
}

// static
bool PlatformThread::CanChangeThreadType(ThreadType /* from */, ThreadType /* to */) {
  return false;
}

size_t PlatformThread::GetDefaultThreadStackSize() {
  return 0;
}

// static
ThreadPriorityForTest PlatformThread::GetCurrentThreadPriorityForTest() {
  NOTIMPLEMENTED();
  return ThreadPriorityForTest::kNormal;
}

}  // namespace base
