// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_semaphore_abi_wrappers.h"

#include <semaphore.h>
#include <time.h>

#include "starboard/shared/modular/starboard_layer_posix_errno_abi_wrappers.h"
#include "starboard/shared/modular/starboard_layer_posix_time_abi_wrappers.h"

typedef struct PosixSemaphorePrivate {
  sem_t semaphore;
} PosixSemaphorePrivate;

// Ensure that the musl semaphore type is large enough to hold our private
// structure. This is a compile-time check to prevent memory corruption.
static_assert(sizeof(musl_sem_t) >= sizeof(PosixSemaphorePrivate),
              "sizeof(musl_sem_t) must be larger or equal to "
              "sizeof(PosixSemaphorePrivate)");

// Macros to simplify accessing the internal semaphore fields from the musl
// type.
#define PTHREAD_INTERNAL_SEMAPHORE(sem_var) \
  &(reinterpret_cast<PosixSemaphorePrivate*>((sem_var)->sem_buffer)->semaphore)

int __abi_wrap_sem_destroy(musl_sem_t* sem) {
  if (!sem) {
    return -1;
  }

  return sem_destroy(PTHREAD_INTERNAL_SEMAPHORE(sem));
}

int __abi_wrap_sem_init(musl_sem_t* sem, int pshared, unsigned int value) {
  if (!sem) {
    return -1;
  }

  return sem_init(PTHREAD_INTERNAL_SEMAPHORE(sem), pshared, value);
}

int __abi_wrap_sem_post(musl_sem_t* sem) {
  if (!sem) {
    return -1;
  }

  return sem_post(PTHREAD_INTERNAL_SEMAPHORE(sem));
}

int __abi_wrap_sem_timedwait(musl_sem_t* sem,
                             const struct musl_timespec* abs_timeout) {
  if (!sem || !abs_timeout) {
    return -1;
  }

  // Convert musl timespec to the native platform timespec.
  struct timespec ts;
  ts.tv_sec = abs_timeout->tv_sec;
  ts.tv_nsec = abs_timeout->tv_nsec;

  return sem_timedwait(PTHREAD_INTERNAL_SEMAPHORE(sem), &ts);
}

int __abi_wrap_sem_wait(musl_sem_t* sem) {
  if (!sem) {
    return -1;
  }

  return sem_wait(PTHREAD_INTERNAL_SEMAPHORE(sem));
}
