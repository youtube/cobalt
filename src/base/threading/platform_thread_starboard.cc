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

#include "base/logging.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_restrictions.h"
#include "starboard/thread.h"

namespace base {

namespace {

struct ThreadParams {
  PlatformThread::Delegate* delegate;
  bool joinable;
};

void* ThreadFunc(void* params) {
  ThreadParams* thread_params = static_cast<ThreadParams*>(params);
  PlatformThread::Delegate* delegate = thread_params->delegate;
  if (!thread_params->joinable) {
    base::ThreadRestrictions::SetSingletonAllowed(false);
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

bool CreateThread(size_t stack_size,
                  SbThreadPriority priority,
                  SbThreadAffinity affinity,
                  bool joinable,
                  const char* name,
                  PlatformThread::Delegate* delegate,
                  PlatformThreadHandle* thread_handle) {
  ThreadParams* params = new ThreadParams;
  params->delegate = delegate;
  params->joinable = joinable;

  SbThread thread = SbThreadCreate(stack_size, priority, affinity, joinable,
                                   name, ThreadFunc, params);
  if (SbThreadIsValid(thread)) {
    if (thread_handle) {
      *thread_handle = PlatformThreadHandle(thread);
    }

    return true;
  }

  return false;
}

inline SbThreadPriority toSbPriority(ThreadPriority priority) {
  return static_cast<SbThreadPriority>(priority);
}
}  // namespace

// static
PlatformThreadId PlatformThread::CurrentId() {
  return SbThreadGetId();
}

// static
PlatformThreadRef PlatformThread::CurrentRef() {
  return PlatformThreadRef(SbThreadGetCurrent());
}

// static
PlatformThreadHandle PlatformThread::CurrentHandle() {
  return PlatformThreadHandle(SbThreadGetCurrent());
}

// static
void PlatformThread::YieldCurrentThread() {
  SbThreadYield();
}

// static
void PlatformThread::Sleep(TimeDelta duration) {
  SbThreadSleep(duration.ToSbTime());
}

// static
void PlatformThread::SetName(const std::string& name) {
  ThreadIdNameManager::GetInstance()->SetName(name);
  SbThreadSetName(name.c_str());
}

// static
const char* PlatformThread::GetName() {
  return ThreadIdNameManager::GetInstance()->GetName(CurrentId());
}

// static
bool PlatformThread::CreateWithPriority(size_t stack_size,
                                        Delegate* delegate,
                                        PlatformThreadHandle* thread_handle,
                                        ThreadPriority priority) {
  return CreateThread(stack_size, toSbPriority(priority), kSbThreadNoAffinity,
                      true /* joinable thread */, NULL, delegate,
                      thread_handle);
}

// static
bool PlatformThread::CreateNonJoinableWithPriority(size_t stack_size,
                                                   Delegate* delegate,
                                                   ThreadPriority priority) {
  return CreateThread(stack_size, toSbPriority(priority), kSbThreadNoAffinity,
                      false /* joinable thread */, NULL, delegate, NULL);
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  // Joining another thread may block the current thread for a long time, since
  // the thread referred to by |thread_handle| may still be running long-lived /
  // blocking tasks.
  AssertBlockingAllowed();
  SbThreadJoin(thread_handle.platform_handle(), NULL);
}

void PlatformThread::Detach(PlatformThreadHandle thread_handle) {
  SbThreadDetach(thread_handle.platform_handle());
}

void PlatformThread::SetCurrentThreadPriority(ThreadPriority priority) {
  NOTIMPLEMENTED();
}

ThreadPriority PlatformThread::GetCurrentThreadPriority() {
  NOTIMPLEMENTED();
  return ThreadPriority::NORMAL;
}

// static
bool PlatformThread::CanIncreaseThreadPriority(ThreadPriority priority) {
  return false;
}

}  // namespace base
