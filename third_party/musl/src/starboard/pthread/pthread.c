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

#include "third_party/musl/src/starboard/pthread/pthread.h"

#include <errno.h>

#if SB_API_VERSION < 16

#include "starboard/common/log.h"
#include "starboard/condition_variable.h"
#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/time.h"

int pthread_mutex_init(pthread_mutex_t* mutext, const pthread_mutexattr_t*) {
  if (SbMutexCreate((SbMutex*)mutext->mutex_buffer)) {
    return 0;
  }
  return EINVAL;
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
  SbMutexResult result = SbMutexAcquire((SbMutex*)mutex->mutex_buffer);
  if (result == kSbMutexAcquired) {
    return 0;
  }
  return EINVAL;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
  if (SbMutexRelease((SbMutex*)mutex->mutex_buffer)) {
    return 0;
  }
  return EINVAL;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
  SbMutexResult result = SbMutexAcquireTry((SbMutex*)mutex->mutex_buffer);
  if (result == kSbMutexAcquired) {
    return 0;
  }
  return EINVAL;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
  if (SbMutexDestroy((SbMutex*)mutex->mutex_buffer)) {
    return 0;
  }
  return EINVAL;
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
  return SbConditionVariableBroadcast((SbConditionVariable*)cond->cond_buffer)
             ? 0
             : -1;
}

int pthread_cond_destroy(pthread_cond_t* cond) {
  return SbConditionVariableDestroy((SbConditionVariable*)cond->cond_buffer)
             ? 0
             : -1;
}

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
  return SbConditionVariableCreate((SbConditionVariable*)cond->cond_buffer,
                                   NULL /* mutex */)
             ? 0
             : -1;
}

int pthread_cond_signal(pthread_cond_t* cond) {
  return SbConditionVariableSignal((SbConditionVariable*)cond->cond_buffer)
             ? 0
             : -1;
}

int pthread_cond_timedwait(pthread_cond_t* cond,
                           pthread_mutex_t* mutex,
                           const struct timespec* t) {
  SbTimeMonotonic now = SbTimeGetMonotonicNow();
  int64_t timeout_duration_microsec = t->tv_sec * 1000000 + t->tv_nsec / 1000;
  timeout_duration_microsec -= now;
  SbConditionVariableResult result = SbConditionVariableWaitTimed(
      (SbConditionVariable*)cond->cond_buffer, (SbMutex*)mutex->mutex_buffer,
      timeout_duration_microsec);
  if (result == kSbConditionVariableSignaled) {
    return 0;
  } else if (result == kSbConditionVariableTimedOut) {
    return ETIMEDOUT;
  }
  return -1;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
  SbConditionVariableResult result = SbConditionVariableWait(
      (SbConditionVariable*)cond->cond_buffer, (SbMutex*)mutex->mutex_buffer);
  if (result == kSbConditionVariableSignaled) {
    return 0;
  }
  return -1;
}

int pthread_condattr_destroy(pthread_condattr_t* attr) {
  // Not supported in Starboard 14/15
  SB_DCHECK(false);
  return -1;
}

int pthread_condattr_init(pthread_condattr_t* attr) {
  // Not supported in Starboard 14/15
  SB_DCHECK(false);
  return -1;
}

int pthread_condattr_getclock(const pthread_condattr_t* attr,
                              clockid_t* clock_id) {
  // Not supported in Starboard 14/15
  SB_DCHECK(false);
  return -1;
}

int pthread_condattr_setclock(pthread_condattr_t* attr, clockid_t clock_id) {
  // Not supported in Starboard 14/15
  SB_DCHECK(false);
  return -1;
}

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
  return SbOnce((SbOnceControl*)once_control->once_buffer, init_routine) ? 0
                                                                         : -1;
}
#endif  // SB_API_VERSION < 16
