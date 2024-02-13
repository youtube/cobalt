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

#include "starboard/mutex.h"

int pthread_mutex_init(pthread_mutex_t *__restrict mutext, const pthread_mutexattr_t *__restrict) {
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

#endif  // SB_API_VERSION < 16
