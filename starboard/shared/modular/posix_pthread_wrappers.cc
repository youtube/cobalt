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

#define INTERNAL_MUTEX(mutex_var) \
  reinterpret_cast<PosixMutexPrivate*>((mutex_var)->mutex_buffer)
#define PTHREAD_INTERNAL_MUTEX(mutex_var) \
  &(reinterpret_cast<PosixMutexPrivate*>((mutex_var)->mutex_buffer)->mutex)
#define PTHREAD_INTERNAL_MUTEX_ATTR(mutex_var)                                \
  &(reinterpret_cast<const PosixMutexAttrPrivate*>((mutex_var)->mutex_buffer) \
        ->mutex_attr)

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
