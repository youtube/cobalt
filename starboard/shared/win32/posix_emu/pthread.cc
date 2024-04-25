// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <errno.h>
#include <process.h>
#include <pthread.h>
#include <cstdint>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/shared/win32/thread_local_internal.h"
#include "starboard/shared/win32/thread_private.h"
#include "starboard/shared/win32/wchar_utils.h"

using starboard::shared::win32::CStringToWString;
using starboard::shared::win32::GetCurrentSbThreadPrivate;
using starboard::shared::win32::GetThreadSubsystemSingleton;
using starboard::shared::win32::SbThreadPrivate;
using starboard::shared::win32::ThreadCreateInfo;
using starboard::shared::win32::ThreadSubsystemSingleton;
using starboard::shared::win32::TlsInternalFree;
using starboard::shared::win32::TlsInternalGetValue;
using starboard::shared::win32::TlsInternalSetValue;

void CallThreadLocalDestructorsMultipleTimes();
void ResetWinError();
int RunThreadLocalDestructors(ThreadSubsystemSingleton* singleton);
int CountTlsObjectsRemaining(ThreadSubsystemSingleton* singleton);

extern "C" {

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
  return 0;
}

int pthread_mutex_init(pthread_mutex_t* mutex,
                       const pthread_mutexattr_t* mutex_attr) {
  if (!mutex) {
    return EINVAL;
  }
  InitializeSRWLock(mutex);
  return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
  if (!mutex) {
    return EINVAL;
  }
  AcquireSRWLockExclusive(mutex);
  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
  if (!mutex) {
    return EINVAL;
  }
  ReleaseSRWLockExclusive(mutex);
  return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
  if (!mutex) {
    return EINVAL;
  }
  bool result = TryAcquireSRWLockExclusive(mutex);
  return result ? 0 : EBUSY;
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
  if (!cond) {
    return -1;
  }
  WakeAllConditionVariable(cond);
  return 0;
}

int pthread_cond_destroy(pthread_cond_t* cond) {
  return 0;
}

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
  if (!cond) {
    return -1;
  }
  InitializeConditionVariable(cond);
  return 0;
}

int pthread_cond_signal(pthread_cond_t* cond) {
  if (!cond) {
    return -1;
  }
  WakeConditionVariable(cond);
  return -0;
}

int pthread_cond_timedwait(pthread_cond_t* cond,
                           pthread_mutex_t* mutex,
                           const struct timespec* t) {
  if (!cond || !mutex) {
    return -1;
  }

  int64_t now_ms = starboard::CurrentMonotonicTime() / 1000;
  int64_t timeout_duration_ms = t->tv_sec * 1000 + t->tv_nsec / 1000000;
  timeout_duration_ms -= now_ms;
  bool result = SleepConditionVariableSRW(cond, mutex, timeout_duration_ms, 0);

  if (result) {
    return 0;
  }

  if (GetLastError() == ERROR_TIMEOUT) {
    return ETIMEDOUT;
  }
  return -1;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
  if (!cond || !mutex) {
    return -1;
  }

  if (SleepConditionVariableSRW(cond, mutex, INFINITE, 0)) {
    return 0;
  }
  return -1;
}

int pthread_condattr_destroy(pthread_condattr_t* attr) {
  SB_DCHECK(false) << "pthread_condattr_destroy not supported on win32";
  return -1;
}

int pthread_condattr_getclock(const pthread_condattr_t* attr,
                              clockid_t* clock_id) {
  SB_DCHECK(false) << "pthread_condattr_getclock not supported on win32";
  return -1;
}

int pthread_condattr_init(pthread_condattr_t* attr) {
  SB_DCHECK(false) << "pthread_condattr_init not supported on win32";
  return -1;
}

int pthread_condattr_setclock(pthread_condattr_t* attr, clockid_t clock_id) {
  SB_DCHECK(false) << "pthread_condattr_setclock not supported on win32";
  return -1;
}

static BOOL CALLBACK OnceTrampoline(PINIT_ONCE once_control,
                                    void* parameter,
                                    void** context) {
  static_cast<void (*)(void)>(parameter)();
  return true;
}

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
  if (!once_control || !init_routine) {
    return -1;
  }
  return InitOnceExecuteOnce(once_control, OnceTrampoline, init_routine, NULL)
             ? 0
             : -1;
}

static unsigned ThreadTrampoline(void* thread_create_info_context) {
  std::unique_ptr<ThreadCreateInfo> info(
      static_cast<ThreadCreateInfo*>(thread_create_info_context));

  ThreadSubsystemSingleton* singleton = GetThreadSubsystemSingleton();
  SbThreadSetLocalValue(singleton->thread_private_key_, &info->thread_private_);
  SbThreadSetName(info->name_.c_str());

  void* result = info->entry_point_(info->user_context_);

  CallThreadLocalDestructorsMultipleTimes();

  SbMutexAcquire(&info->thread_private_.mutex_);
  info->thread_private_.result_ = result;
  info->thread_private_.result_is_valid_ = true;
  SbConditionVariableSignal(&info->thread_private_.condition_);
  while (info->thread_private_.wait_for_join_) {
    SbConditionVariableWait(&info->thread_private_.condition_,
                            &info->thread_private_.mutex_);
  }
  SbMutexRelease(&info->thread_private_.mutex_);

  return 0;
}

int pthread_create(pthread_t* thread,
                   const pthread_attr_t* attr,
                   void* (*start_routine)(void*),
                   void* arg) {
  if (start_routine == NULL) {
    return -1;
  }
  ThreadCreateInfo* info = new ThreadCreateInfo();

  info->entry_point_ = start_routine;
  info->user_context_ = arg;
  info->thread_private_.wait_for_join_ = true;

  // Create the thread suspended, and then resume once ThreadCreateInfo::handle_
  // has been set, so that it's always valid in the ThreadCreateInfo
  // destructor.
  uintptr_t handle =
      _beginthreadex(NULL, 0, ThreadTrampoline, info, CREATE_SUSPENDED, NULL);
  SB_DCHECK(handle);
  info->thread_private_.handle_ = reinterpret_cast<HANDLE>(handle);
  ResetWinError();

  ResumeThread(info->thread_private_.handle_);

  *thread = &info->thread_private_;
  return 0;
}

int pthread_join(pthread_t thread, void** value_ptr) {
  if (thread == NULL) {
    return -1;
  }

  SbThreadPrivate* thread_private = static_cast<SbThreadPrivate*>(thread);

  SbMutexAcquire(&thread_private->mutex_);
  if (!thread_private->wait_for_join_) {
    // Thread has already been detached.
    SbMutexRelease(&thread_private->mutex_);
    return -1;
  }
  while (!thread_private->result_is_valid_) {
    SbConditionVariableWait(&thread_private->condition_,
                            &thread_private->mutex_);
  }
  thread_private->wait_for_join_ = false;
  SbConditionVariableSignal(&thread_private->condition_);
  if (value_ptr != NULL) {
    *value_ptr = thread_private->result_;
  }
  SbMutexRelease(&thread_private->mutex_);
  return 0;
}

int pthread_detach(pthread_t thread) {
  if (thread == NULL) {
    return -1;
  }
  SbThreadPrivate* thread_private = static_cast<SbThreadPrivate*>(thread);

  SbMutexAcquire(&thread_private->mutex_);
  thread_private->wait_for_join_ = false;
  SbConditionVariableSignal(&thread_private->condition_);
  SbMutexRelease(&thread_private->mutex_);
  return 0;
}

pthread_t pthread_self() {
  return GetCurrentSbThreadPrivate();
}

int pthread_equal(pthread_t t1, pthread_t t2) {
  return t1 == t2;
}

int pthread_key_create(pthread_key_t* key, void (*dtor)(void*)) {
  ThreadSubsystemSingleton* singleton = GetThreadSubsystemSingleton();
  *key = SbThreadCreateLocalKeyInternal(dtor, singleton);
  return 0;
}

int pthread_key_delete(pthread_key_t key) {
  if (!key) {
    return -1;
  }
  // To match pthreads, the thread local pointer for the key is set to null
  // so that a supplied destructor doesn't run.
  SbThreadSetLocalValue(reinterpret_cast<SbThreadLocalKey>(key), nullptr);
  DWORD tls_index = static_cast<SbThreadLocalKeyPrivate*>(key)->tls_index;
  ThreadSubsystemSingleton* singleton = GetThreadSubsystemSingleton();

  SbMutexAcquire(&singleton->mutex_);
  singleton->thread_local_keys_.erase(tls_index);
  SbMutexRelease(&singleton->mutex_);

  TlsInternalFree(tls_index);
  free(key);
  return 0;
}

void* pthread_getspecific(pthread_key_t key) {
  if (!key) {
    return NULL;
  }
  DWORD tls_index = static_cast<SbThreadLocalKeyPrivate*>(key)->tls_index;
  return TlsInternalGetValue(tls_index);
}

int pthread_setspecific(pthread_key_t key, const void* value) {
  if (!key) {
    return -1;
  }
  DWORD tls_index = static_cast<SbThreadLocalKeyPrivate*>(key)->tls_index;
  return TlsInternalSetValue(tls_index, const_cast<void*>(value)) ? 0 : -1;
}

int pthread_setname_np(pthread_t thread, const char* name) {
  SbThreadPrivate* thread_private = static_cast<SbThreadPrivate*>(thread);
  std::wstring wname = CStringToWString(name);

  HRESULT hr = SetThreadDescription(thread_private->handle_, wname.c_str());
  if (FAILED(hr)) {
    return -1;
  }
  // We store the thread name in our own TLS context as well as telling
  // the OS because it's much easier to retrieve from our own TLS context.
  thread_private->name_ = name;

  return 0;
}

int pthread_getname_np(pthread_t thread, char* name, size_t len) {
  SbThreadPrivate* thread_private = static_cast<SbThreadPrivate*>(thread);
  starboard::strlcpy(name, thread_private->name_.c_str(), len);
  return 0;
}
}  // extern "C"
