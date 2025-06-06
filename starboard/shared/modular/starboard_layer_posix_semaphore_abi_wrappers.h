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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_SEMAPHORE_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_SEMAPHORE_ABI_WRAPPERS_H_

#include <stdint.h>

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/shared/modular/starboard_layer_posix_time_abi_wrappers.h"

#ifdef __cplusplus
extern "C" {
#endif

// Define the maximum size for the native semaphore type. This ensures that
// our musl-compatible type has enough space to hold the platform's semaphore.
#define MUSL_SEM_MAX_SIZE 64

// A musl-compatible semaphore type. It uses a buffer to hold the underlying
// platform-specific semaphore object.
typedef union musl_sem_t {
  uint8_t sem_buffer[MUSL_SEM_MAX_SIZE];
  void* ptr;
} musl_sem_t;

// Function declarations for the semaphore ABI wrappers. These functions provide
// a stable ABI for semaphore operations, translating between the
// musl-compatible types and the native platform types.

SB_EXPORT int __abi_wrap_sem_destroy(musl_sem_t* sem);
SB_EXPORT int __abi_wrap_sem_init(musl_sem_t* sem,
                                  int pshared,
                                  unsigned int value);
SB_EXPORT int __abi_wrap_sem_post(musl_sem_t* sem);
SB_EXPORT int __abi_wrap_sem_timedwait(musl_sem_t* sem,
                                       const struct musl_timespec* abs_timeout);
SB_EXPORT int __abi_wrap_sem_wait(musl_sem_t* sem);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_SEMAPHORE_ABI_WRAPPERS_H_
