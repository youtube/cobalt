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
#include <pthread.h>
#include <cstdint>

#include "starboard/common/log.h"
#include "starboard/common/time.h"

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

}  // extern "C"
