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

#include "starboard/shared/modular/posix_pthread_wrappers.h"

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

int __wrap_pthread_mutex_destroy(musl_pthread_mutex_t* mutex) {
  if (!mutex) {
    return EINVAL;
  }

  if (!IsInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state))) {
    // If the mutex is not initialized there is nothing to destroy.
    return 0;
  }

  return pthread_mutex_destroy(PTHREAD_INTERNAL_MUTEX(mutex));
}

int __wrap_pthread_mutex_init(musl_pthread_mutex_t* mutex,
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

int __wrap_pthread_mutex_lock(musl_pthread_mutex_t* mutex) {
  if (!mutex) {
    return EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state))) {
    *PTHREAD_INTERNAL_MUTEX(mutex) = PTHREAD_MUTEX_INITIALIZER;
    SetInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state));
  }

  return pthread_mutex_lock(PTHREAD_INTERNAL_MUTEX(mutex));
}

int __wrap_pthread_mutex_unlock(musl_pthread_mutex_t* mutex) {
  if (!mutex) {
    return EINVAL;
  }

  if (!IsInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state))) {
    // If the mutex is not initialized there is nothing to release.
    return EINVAL;
  }
  return pthread_mutex_unlock(PTHREAD_INTERNAL_MUTEX(mutex));
}

int __wrap_pthread_mutex_trylock(musl_pthread_mutex_t* mutex) {
  if (!mutex) {
    return EINVAL;
  }

  if (!EnsureInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state))) {
    *PTHREAD_INTERNAL_MUTEX(mutex) = PTHREAD_MUTEX_INITIALIZER;
    SetInitialized(&(INTERNAL_MUTEX(mutex)->initialized_state));
  }

  return pthread_mutex_trylock(PTHREAD_INTERNAL_MUTEX(mutex));
}

int __wrap_pthread_cond_broadcast(musl_pthread_cond_t* cond) {
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

int __wrap_pthread_cond_destroy(musl_pthread_cond_t* cond) {
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

int __wrap_pthread_cond_init(musl_pthread_cond_t* cond,
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

int __wrap_pthread_cond_signal(musl_pthread_cond_t* cond) {
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

int __wrap_pthread_cond_timedwait(musl_pthread_cond_t* cond,
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

int __wrap_pthread_cond_wait(musl_pthread_cond_t* cond,
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

int __wrap_pthread_condattr_destroy(musl_pthread_condattr_t* attr) {
  if (!attr) {
    return EINVAL;
  }
  return pthread_condattr_destroy(PTHREAD_INTERNAL_CONDITION_ATTR(attr));
}

int __wrap_pthread_condattr_getclock(const musl_pthread_condattr_t* attr,
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

int __wrap_pthread_condattr_init(musl_pthread_condattr_t* attr) {
  if (!attr) {
    return EINVAL;
  }
  return pthread_condattr_init(PTHREAD_INTERNAL_CONDITION_ATTR(attr));
}

int __wrap_pthread_condattr_setclock(musl_pthread_condattr_t* attr,
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
