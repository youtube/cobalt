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

}  // extern "C"
