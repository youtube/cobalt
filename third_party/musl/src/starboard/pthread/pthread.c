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

typedef struct pthread_attr_impl_t {
  size_t stack_size;
  int detach_state;
} pthread_attr_impl_t;

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

int pthread_create(pthread_t* thread,
                   const pthread_attr_t* attr,
                   void* (*start_routine)(void*),
                   void* arg) {
  int stack_size = 0;
  bool joinable = true;
  if (attr != NULL) {
    stack_size = ((pthread_attr_impl_t*)(attr->attr_buffer))->stack_size;
    if ((((pthread_attr_impl_t*)(attr->attr_buffer))->detach_state ==
         PTHREAD_CREATE_DETACHED)) {
      joinable = false;
    }
  }

  SbThread starboard_thread =
      SbThreadCreate(stack_size, kSbThreadNoPriority, kSbThreadNoAffinity,
                     joinable, NULL, start_routine, arg);
  if (SbThreadIsValid(thread)) {
    *thread = starboard_thread;
    return 0;
  }
  return EINVAL;
}

int pthread_join(pthread_t thread, void** value_ptr) {
  return SbThreadJoin(thread, value_ptr) ? 0 : EINVAL;
}

int pthread_detach(pthread_t thread) {
  SbThreadDetach(thread);
  return 0;
}

pthread_t pthread_self() {
  return SbThreadGetCurrent();
}

int pthread_equal(pthread_t t1, pthread_t t2) {
  return SbThreadIsEqual(t1, t2);
}

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*)) {
  SbThreadLocalKey sb_key = SbThreadCreateLocalKey(destructor);
  if (SbThreadIsValidLocalKey(sb_key)) {
    *key = sb_key;
    return 0;
  }
  return -1;
}

int pthread_key_delete(pthread_key_t key) {
  SbThreadDestroyLocalKey((SbThreadLocalKey)key);
  return 0;
}

void* pthread_getspecific(pthread_key_t key) {
  return SbThreadGetLocalValue((SbThreadLocalKey)key);
}

int pthread_setspecific(pthread_key_t key, const void* value) {
  return SbThreadSetLocalValue((SbThreadLocalKey)key, value) ? 0 : -1;
}

int pthread_setname_np(pthread_t thread, const char* name) {
  // Starboard 14/15 can only set thread name for the current thread
  if (SbThreadGetCurrent() != thread) {
    SB_DCHECK(false);
    return -1;
  }
  SbThreadSetName(name);
  return 0;
}

int pthread_getname_np(pthread_t thread, char* name, size_t len) {
  // Starboard 14/15 can only get the thread name for the current thread
  if (SbThreadGetCurrent() != thread) {
    SB_DCHECK(false);
    return -1;
  }
  SbThreadGetName(name, len);
  return 0;
}

int pthread_attr_init(pthread_attr_t* attr) {
  memset(attr, 0, sizeof(pthread_attr_t)); 
  return 0;
}

int pthread_attr_destroy(pthread_attr_t* attr) {
  return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stack_size) {
  *stack_size = ((pthread_attr_impl_t*)(attr->attr_buffer))->stack_size;
  return 0;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stack_size) {
  ((pthread_attr_impl_t*)(attr->attr_buffer))->stack_size = stack_size;
  return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detach_state) {
  *detach_state = ((pthread_attr_impl_t*)(attr->attr_buffer))->detach_state;
  return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t* attr, int detach_state) {
  ((pthread_attr_impl_t*)(attr->attr_buffer))->detach_state = detach_state;
  return 0;
}

#endif  // SB_API_VERSION < 16
