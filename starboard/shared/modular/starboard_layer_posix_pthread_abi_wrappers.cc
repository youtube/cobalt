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
    return EINVAL;
  }

  if (!IsInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state))) {
    // If the mutex is not initialized there is nothing to destroy.
    return 0;
  }

  return pthread_mutex_destroy(PTHREAD_INTERNAL_MUTEX(mutex));
}

int __abi_wrap_pthread_mutex_init(musl_pthread_mutex_t* mutex,
                                  const musl_pthread_mutexattr_t* mutex_attr) {
  if (!mutex) {
    return EINVAL;
  }

  *PTHREAD_INTERNAL_MUTEX(mutex) = PTHREAD_MUTEX_INITIALIZER;
  SetInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state));

  const pthread_mutexattr_t* tmp = nullptr;
  if (mutex_attr) {
    tmp = PTHREAD_INTERNAL_MUTEX_ATTR(mutex_attr);
  }
  return pthread_mutex_init(PTHREAD_INTERNAL_MUTEX(mutex), tmp);
}

int __abi_wrap_pthread_mutex_lock(musl_pthread_mutex_t* mutex) {
  if (!mutex) {
    return EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state))) {
    *PTHREAD_INTERNAL_MUTEX(mutex) = PTHREAD_MUTEX_INITIALIZER;
    SetInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state));
  }

  return pthread_mutex_lock(PTHREAD_INTERNAL_MUTEX(mutex));
}

int __abi_wrap_pthread_mutex_unlock(musl_pthread_mutex_t* mutex) {
  if (!mutex) {
    return EINVAL;
  }

  if (!IsInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state))) {
    // If the mutex is not initialized there is nothing to release.
    return EINVAL;
  }
  return pthread_mutex_unlock(PTHREAD_INTERNAL_MUTEX(mutex));
}

int __abi_wrap_pthread_mutex_trylock(musl_pthread_mutex_t* mutex) {
  if (!mutex) {
    return EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state))) {
    *PTHREAD_INTERNAL_MUTEX(mutex) = PTHREAD_MUTEX_INITIALIZER;
    SetInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state));
  }

  return pthread_mutex_trylock(PTHREAD_INTERNAL_MUTEX(mutex));
}

int __abi_wrap_pthread_cond_broadcast(musl_pthread_cond_t* cond) {
  if (!cond) {
    return EINVAL;
  }
  if (!EnsureInitialized(&(INTERNAL_CONDITION(cond)->initialized_state))) {
    if (pthread_cond_init(PTHREAD_INTERNAL_CONDITION(cond), NULL) != 0) {
      return EINVAL;
    }

    SetInitialized(&(INTERNAL_CONDITION(cond)->initialized_state));
  }

  return pthread_cond_broadcast(PTHREAD_INTERNAL_CONDITION(cond));
}

int __abi_wrap_pthread_cond_destroy(musl_pthread_cond_t* cond) {
  if (!cond) {
    return EINVAL;
  }
  if (!IsInitialized(&(INTERNAL_CONDITION(cond)->initialized_state))) {
    // If the condition variable is not initialized yet, then there is nothing
    // to destroy so vacuously return true.
    return 0;
  }

  return pthread_cond_destroy(PTHREAD_INTERNAL_CONDITION(cond));
}

int __abi_wrap_pthread_cond_init(musl_pthread_cond_t* cond,
                                 const musl_pthread_condattr_t* attr) {
  if (!cond) {
    return EINVAL;
  }
  SetInitialized(&(INTERNAL_CONDITION(cond)->initialized_state));

  if (attr) {
    return pthread_cond_init(PTHREAD_INTERNAL_CONDITION(cond),
                             CONST_PTHREAD_INTERNAL_CONDITION_ATTR(attr));
  } else {
    return pthread_cond_init(PTHREAD_INTERNAL_CONDITION(cond), NULL);
  }
}

int __abi_wrap_pthread_cond_signal(musl_pthread_cond_t* cond) {
  if (!cond) {
    return EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_CONDITION(cond)->initialized_state))) {
    if (pthread_cond_init(PTHREAD_INTERNAL_CONDITION(cond), NULL) != 0) {
      return EINVAL;
    }

    SetInitialized(&(INTERNAL_CONDITION(cond)->initialized_state));
  }

  return pthread_cond_signal(PTHREAD_INTERNAL_CONDITION(cond));
}

int __abi_wrap_pthread_cond_timedwait(musl_pthread_cond_t* cond,
                                      musl_pthread_mutex_t* mutex,
                                      const struct musl_timespec* t) {
  if (!cond || !mutex || !t) {
    return EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_CONDITION(cond)->initialized_state))) {
    if (pthread_cond_init(PTHREAD_INTERNAL_CONDITION(cond), NULL) != 0) {
      return EINVAL;
    }

    SetInitialized(&(INTERNAL_CONDITION(cond)->initialized_state));
  }

  struct timespec ts;  // The type from platform toolchain.
  ts.tv_sec = t->tv_sec;
  ts.tv_nsec = t->tv_nsec;

  int ret = pthread_cond_timedwait(PTHREAD_INTERNAL_CONDITION(cond),
                                   PTHREAD_INTERNAL_MUTEX(mutex), &ts);
  return ret;
}

int __abi_wrap_pthread_cond_wait(musl_pthread_cond_t* cond,
                                 musl_pthread_mutex_t* mutex) {
  if (!cond || !mutex) {
    return EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_CONDITION(cond)->initialized_state))) {
    if (pthread_cond_init(PTHREAD_INTERNAL_CONDITION(cond), NULL) != 0) {
      return EINVAL;
    }

    SetInitialized(&(INTERNAL_CONDITION(cond)->initialized_state));
  }

  return pthread_cond_wait(PTHREAD_INTERNAL_CONDITION(cond),
                           PTHREAD_INTERNAL_MUTEX(mutex));
}

int __abi_wrap_pthread_condattr_destroy(musl_pthread_condattr_t* attr) {
  if (!attr) {
    return EINVAL;
  }
  return pthread_condattr_destroy(PTHREAD_INTERNAL_CONDITION_ATTR(attr));
}

int __abi_wrap_pthread_condattr_getclock(const musl_pthread_condattr_t* attr,
                                         clockid_t* clock_id) {
  if (!attr) {
    return EINVAL;
  }

#if !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
  return pthread_condattr_getclock(CONST_PTHREAD_INTERNAL_CONDITION_ATTR(attr),
                                   clock_id);
#else
  SB_DCHECK(false) << "pthread_condattr_getclock unsupported";
  return EINVAL;
#endif
}

int __abi_wrap_pthread_condattr_init(musl_pthread_condattr_t* attr) {
  if (!attr) {
    return EINVAL;
  }
  return pthread_condattr_init(PTHREAD_INTERNAL_CONDITION_ATTR(attr));
}

int __abi_wrap_pthread_condattr_setclock(musl_pthread_condattr_t* attr,
                                         clockid_t clock_id) {
  if (!attr) {
    return EINVAL;
  }
#if !SB_HAS_QUIRK(NO_CONDATTR_SETCLOCK_SUPPORT)
  return pthread_condattr_setclock(PTHREAD_INTERNAL_CONDITION_ATTR(attr),
                                   clock_id);
#else
  SB_DCHECK(false) << "pthread_condattr_setclock unsupported";
  return EINVAL;
#endif
}

int __abi_wrap_pthread_once(musl_pthread_once_t* once_control,
                            void (*init_routine)(void)) {
  if (!once_control || !init_routine) {
    return EINVAL;
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
  return pthread_create(reinterpret_cast<pthread_t*>(thread),
                        CONST_PTHREAD_INTERNAL_ATTR(attr), start_routine, arg);
}

int __abi_wrap_pthread_join(musl_pthread_t thread, void** value_ptr) {
  return pthread_join(reinterpret_cast<pthread_t>(thread), value_ptr);
}

int __abi_wrap_pthread_detach(musl_pthread_t thread) {
  return pthread_detach(reinterpret_cast<pthread_t>(thread));
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
  if (pthread_key_create(&private_key->key, destructor) != 0) {
    delete private_key;
    return -1;
  }
  *key = private_key;
  return 0;
}

int __abi_wrap_pthread_key_delete(musl_pthread_key_t key) {
  if (!key) {
    return -1;
  }

  int res = pthread_key_delete(
      reinterpret_cast<PosixThreadLocalKeyPrivate*>(key)->key);
  delete reinterpret_cast<PosixThreadLocalKeyPrivate*>(key);
  return res;
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
    return -1;
  }
  return pthread_setspecific(
      reinterpret_cast<PosixThreadLocalKeyPrivate*>(key)->key, value);
}

int __abi_wrap_pthread_setname_np(musl_pthread_t thread, const char* name) {
  return pthread_setname_np(reinterpret_cast<pthread_t>(thread), name);
}

int __abi_wrap_pthread_getname_np(musl_pthread_t thread,
                                  char* name,
                                  size_t len) {
  return pthread_getname_np(reinterpret_cast<pthread_t>(thread), name, len);
}
