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

#include "starboard/shared/modular/starboard_layer_posix_pthread_abi_wrappers.h"

#include <pthread.h>

#include "starboard/shared/modular/starboard_layer_posix_errno_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_time_abi_wrappers.h"
#include "starboard/shared/pthread/is_success.h"
#include "starboard/shared/starboard/lazy_initialization_internal.h"

using starboard::shared::starboard::EnsureInitialized;
using starboard::shared::starboard::IsInitialized;
using starboard::shared::starboard::SetInitialized;

typedef struct PosixMutexPrivate {
  InitializedState initialized_state;
  pthread_mutex_t mutex;
} PosixMutexPrivate;

typedef struct PosixMutexAttrPrivate {
  InitializedState initialized_state;
  pthread_mutexattr_t mutex_attr;
} PosixMutexAttrPrivate;

typedef struct PosixCondPrivate {
  InitializedState initialized_state;
  pthread_cond_t cond;
} PosixCondPrivate;

typedef struct PosixCondAttrPrivate {
  pthread_condattr_t cond_attr;
} PosixCondAttrPrivate;

typedef struct PosixAttrPrivate {
  pthread_attr_t attr;
} PosixAttrPrivate;

typedef struct PosixOncePrivate {
  InitializedState initialized_state;
  pthread_once_t once;
} PosixOncePrivate;

typedef struct PosixThreadLocalKeyPrivate {
  // The underlying thread-local variable handle.
  pthread_key_t key;
} PosixThreadLocalKeyPrivate;

#define INTERNAL_MUTEX(mutex_var) \
  reinterpret_cast<PosixMutexPrivate*>((mutex_var)->mutex_buffer)
#define PTHREAD_INTERNAL_MUTEX(mutex_var) \
  &(reinterpret_cast<PosixMutexPrivate*>((mutex_var)->mutex_buffer)->mutex)
#define PTHREAD_INTERNAL_MUTEX_ATTR(mutex_var)                                \
  &(reinterpret_cast<const PosixMutexAttrPrivate*>((mutex_var)->mutex_buffer) \
        ->mutex_attr)
#define INTERNAL_CONDITION(condition_var) \
  reinterpret_cast<PosixCondPrivate*>((condition_var)->cond_buffer)
#define PTHREAD_INTERNAL_CONDITION(condition_var) \
  &(reinterpret_cast<PosixCondPrivate*>((condition_var)->cond_buffer)->cond)
#define CONST_PTHREAD_INTERNAL_CONDITION_ATTR(condition_attr) \
  &(reinterpret_cast<const PosixCondAttrPrivate*>(            \
        (condition_attr)->cond_attr_buffer)                   \
        ->cond_attr)
#define PTHREAD_INTERNAL_CONDITION_ATTR(condition_attr) \
  &(reinterpret_cast<PosixCondAttrPrivate*>(            \
        (condition_attr)->cond_attr_buffer)             \
        ->cond_attr)
#define PTHREAD_INTERNAL_ATTR(condition_attr) \
  &(reinterpret_cast<PosixAttrPrivate*>((attr)->attr_buffer)->attr)
#define CONST_PTHREAD_INTERNAL_ATTR(condition_attr) \
  &(reinterpret_cast<const PosixAttrPrivate*>((attr)->attr_buffer)->attr)

#define INTERNAL_ONCE(once_control) \
  reinterpret_cast<PosixOncePrivate*>((once_control)->once_buffer)

int __abi_wrap_pthread_mutex_destroy(musl_pthread_mutex_t* mutex) {
  if (!mutex) {
    return MUSL_EINVAL;
  }

  if (!IsInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state))) {
    // If the mutex is not initialized there is nothing to destroy.
    return 0;
  }

  int ret = pthread_mutex_destroy(PTHREAD_INTERNAL_MUTEX(mutex));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_mutex_init(musl_pthread_mutex_t* mutex,
                                  const musl_pthread_mutexattr_t* mutex_attr) {
  if (!mutex) {
    return MUSL_EINVAL;
  }

  *PTHREAD_INTERNAL_MUTEX(mutex) = PTHREAD_MUTEX_INITIALIZER;
  SetInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state));

  const pthread_mutexattr_t* tmp = nullptr;
  if (mutex_attr) {
    tmp = PTHREAD_INTERNAL_MUTEX_ATTR(mutex_attr);
  }
  int ret = pthread_mutex_init(PTHREAD_INTERNAL_MUTEX(mutex), tmp);
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_mutex_lock(musl_pthread_mutex_t* mutex) {
  if (!mutex) {
    return MUSL_EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state))) {
    *PTHREAD_INTERNAL_MUTEX(mutex) = PTHREAD_MUTEX_INITIALIZER;
    SetInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state));
  }

  int ret = pthread_mutex_lock(PTHREAD_INTERNAL_MUTEX(mutex));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_mutex_unlock(musl_pthread_mutex_t* mutex) {
  if (!mutex) {
    return MUSL_EINVAL;
  }

  if (!IsInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state))) {
    // If the mutex is not initialized there is nothing to release.
    return MUSL_EINVAL;
  }
  int ret = pthread_mutex_unlock(PTHREAD_INTERNAL_MUTEX(mutex));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_mutex_trylock(musl_pthread_mutex_t* mutex) {
  if (!mutex) {
    return MUSL_EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state))) {
    *PTHREAD_INTERNAL_MUTEX(mutex) = PTHREAD_MUTEX_INITIALIZER;
    SetInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state));
  }

  int ret = pthread_mutex_trylock(PTHREAD_INTERNAL_MUTEX(mutex));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_cond_broadcast(musl_pthread_cond_t* cond) {
  if (!cond) {
    return MUSL_EINVAL;
  }
  if (!EnsureInitialized(&(INTERNAL_CONDITION(cond)->initialized_state))) {
    if (pthread_cond_init(PTHREAD_INTERNAL_CONDITION(cond), NULL) != 0) {
      return MUSL_EINVAL;
    }

    SetInitialized(&(INTERNAL_CONDITION(cond)->initialized_state));
  }

  int ret = pthread_cond_broadcast(PTHREAD_INTERNAL_CONDITION(cond));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_cond_destroy(musl_pthread_cond_t* cond) {
  if (!cond) {
    return MUSL_EINVAL;
  }
  if (!IsInitialized(&(INTERNAL_CONDITION(cond)->initialized_state))) {
    // If the condition variable is not initialized yet, then there is nothing
    // to destroy so vacuously return true.
    return 0;
  }

  int ret = pthread_cond_destroy(PTHREAD_INTERNAL_CONDITION(cond));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_cond_init(musl_pthread_cond_t* cond,
                                 const musl_pthread_condattr_t* attr) {
  if (!cond) {
    return MUSL_EINVAL;
  }
  SetInitialized(&(INTERNAL_CONDITION(cond)->initialized_state));

  int ret = EINVAL;
  if (attr) {
    ret = pthread_cond_init(PTHREAD_INTERNAL_CONDITION(cond),
                            CONST_PTHREAD_INTERNAL_CONDITION_ATTR(attr));

  } else {
    ret = pthread_cond_init(PTHREAD_INTERNAL_CONDITION(cond), NULL);
  }

  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_cond_signal(musl_pthread_cond_t* cond) {
  if (!cond) {
    return MUSL_EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_CONDITION(cond)->initialized_state))) {
    if (pthread_cond_init(PTHREAD_INTERNAL_CONDITION(cond), NULL) != 0) {
      return MUSL_EINVAL;
    }

    SetInitialized(&(INTERNAL_CONDITION(cond)->initialized_state));
  }

  int ret = pthread_cond_signal(PTHREAD_INTERNAL_CONDITION(cond));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_cond_timedwait(musl_pthread_cond_t* cond,
                                      musl_pthread_mutex_t* mutex,
                                      const struct musl_timespec* t) {
  if (!cond || !mutex || !t) {
    return MUSL_EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_CONDITION(cond)->initialized_state))) {
    if (pthread_cond_init(PTHREAD_INTERNAL_CONDITION(cond), NULL) != 0) {
      return MUSL_EINVAL;
    }

    SetInitialized(&(INTERNAL_CONDITION(cond)->initialized_state));
  }

  struct timespec ts;  // The type from platform toolchain.
  ts.tv_sec = t->tv_sec;
  ts.tv_nsec = t->tv_nsec;

  int ret = pthread_cond_timedwait(PTHREAD_INTERNAL_CONDITION(cond),
                                   PTHREAD_INTERNAL_MUTEX(mutex), &ts);
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_cond_wait(musl_pthread_cond_t* cond,
                                 musl_pthread_mutex_t* mutex) {
  if (!cond || !mutex) {
    return MUSL_EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_CONDITION(cond)->initialized_state))) {
    if (pthread_cond_init(PTHREAD_INTERNAL_CONDITION(cond), NULL) != 0) {
      return MUSL_EINVAL;
    }

    SetInitialized(&(INTERNAL_CONDITION(cond)->initialized_state));
  }

  int ret = pthread_cond_wait(PTHREAD_INTERNAL_CONDITION(cond),
                              PTHREAD_INTERNAL_MUTEX(mutex));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_condattr_destroy(musl_pthread_condattr_t* attr) {
  if (!attr) {
    return MUSL_EINVAL;
  }
  int ret = pthread_condattr_destroy(PTHREAD_INTERNAL_CONDITION_ATTR(attr));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_condattr_getclock(const musl_pthread_condattr_t* attr,
                                         clockid_t* clock_id) {
  if (!attr) {
    return MUSL_EINVAL;
  }

  int ret = pthread_condattr_getclock(
      CONST_PTHREAD_INTERNAL_CONDITION_ATTR(attr), clock_id);
  *clock_id = clock_id_to_musl_clock_id(*clock_id);
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_condattr_init(musl_pthread_condattr_t* attr) {
  if (!attr) {
    return MUSL_EINVAL;
  }
  int ret = pthread_condattr_init(PTHREAD_INTERNAL_CONDITION_ATTR(attr));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_condattr_setclock(musl_pthread_condattr_t* attr,
                                         clockid_t clock_id) {
  if (!attr) {
    return MUSL_EINVAL;
  }

  clock_id = musl_clock_id_to_clock_id(clock_id);
  int ret = pthread_condattr_setclock(PTHREAD_INTERNAL_CONDITION_ATTR(attr),
                                      clock_id);
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_once(musl_pthread_once_t* once_control,
                            void (*init_routine)(void)) {
  if (!once_control || !init_routine) {
    return MUSL_EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_ONCE(once_control)->initialized_state))) {
    init_routine();
    SetInitialized(&(INTERNAL_ONCE(once_control)->initialized_state));
  }
  return 0;
}

int __abi_wrap_pthread_create(musl_pthread_t* thread,
                              const musl_pthread_attr_t* attr,
                              void* (*start_routine)(void*),
                              void* arg) {
  int ret =
      pthread_create(reinterpret_cast<pthread_t*>(thread),
                     CONST_PTHREAD_INTERNAL_ATTR(attr), start_routine, arg);
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_join(musl_pthread_t thread, void** value_ptr) {
  int ret = pthread_join(reinterpret_cast<pthread_t>(thread), value_ptr);
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_detach(musl_pthread_t thread) {
  int ret = pthread_detach(reinterpret_cast<pthread_t>(thread));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_equal(musl_pthread_t t1, musl_pthread_t t2) {
  return pthread_equal(reinterpret_cast<pthread_t>(t1),
                       reinterpret_cast<pthread_t>(t2));
}

musl_pthread_t __abi_wrap_pthread_self() {
  return reinterpret_cast<musl_pthread_t>(pthread_self());
}

int __abi_wrap_pthread_key_create(musl_pthread_key_t* key,
                                  void (*destructor)(void*)) {
  PosixThreadLocalKeyPrivate* private_key = new PosixThreadLocalKeyPrivate();
  int ret = pthread_key_create(&private_key->key, destructor);
  if (ret != 0) {
    delete private_key;
    return errno_to_musl_errno(ret);
  }
  *key = private_key;
  return 0;
}

int __abi_wrap_pthread_key_delete(musl_pthread_key_t key) {
  if (!key) {
    return MUSL_EINVAL;
  }

  int ret = pthread_key_delete(
      reinterpret_cast<PosixThreadLocalKeyPrivate*>(key)->key);
  delete reinterpret_cast<PosixThreadLocalKeyPrivate*>(key);
  return errno_to_musl_errno(ret);
}

void* __abi_wrap_pthread_getspecific(musl_pthread_key_t key) {
  if (!key) {
    return NULL;
  }

  return pthread_getspecific(
      reinterpret_cast<PosixThreadLocalKeyPrivate*>(key)->key);
}

int __abi_wrap_pthread_setspecific(musl_pthread_key_t key, const void* value) {
  if (!key) {
    return MUSL_EINVAL;
  }
  int ret = pthread_setspecific(
      reinterpret_cast<PosixThreadLocalKeyPrivate*>(key)->key, value);
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_setname_np(musl_pthread_t thread, const char* name) {
  int ret = pthread_setname_np(reinterpret_cast<pthread_t>(thread), name);
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_getname_np(musl_pthread_t thread,
                                  char* name,
                                  size_t len) {
  return pthread_getname_np(reinterpret_cast<pthread_t>(thread), name, len);
}

int __abi_wrap_pthread_attr_init(musl_pthread_attr_t* attr) {
  int ret = pthread_attr_init(PTHREAD_INTERNAL_ATTR(attr));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_attr_destroy(musl_pthread_attr_t* attr) {
  int ret = pthread_attr_destroy(PTHREAD_INTERNAL_ATTR(attr));
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_attr_getstacksize(const musl_pthread_attr_t* attr,
                                         size_t* stack_size) {
  int ret =
      pthread_attr_getstacksize(CONST_PTHREAD_INTERNAL_ATTR(attr), stack_size);
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_attr_setstacksize(musl_pthread_attr_t* attr,
                                         size_t stack_size) {
  int ret = pthread_attr_setstacksize(PTHREAD_INTERNAL_ATTR(attr), stack_size);
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_attr_getdetachstate(const musl_pthread_attr_t* attr,
                                           int* detach_state) {
  int ret = pthread_attr_getdetachstate(CONST_PTHREAD_INTERNAL_ATTR(attr),
                                        detach_state);
  return errno_to_musl_errno(ret);
}

int __abi_wrap_pthread_attr_setdetachstate(musl_pthread_attr_t* attr,
                                           int detach_state) {
  int d = PTHREAD_CREATE_JOINABLE;
  if (detach_state == MUSL_PTHREAD_CREATE_DETACHED) {
    d = PTHREAD_CREATE_DETACHED;
  }
  int ret = pthread_attr_setdetachstate(PTHREAD_INTERNAL_ATTR(attr), d);
  return errno_to_musl_errno(ret);
}
