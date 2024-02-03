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

#ifndef STARBOARD_SHARED_MODULAR_POSIX_PTHREAD_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_POSIX_PTHREAD_WRAPPERS_H_

#include <stdint.h>

#include "starboard/configuration.h"
#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// Max size of the native mutex type.
#define MUSL_MUTEX_MAX_SIZE 80

typedef union musl_pthread_mutex_t {
  uint8_t mutex_buffer[MUSL_MUTEX_MAX_SIZE];
  void* ptr;
} musl_pthread_mutex_t;

#define MUSL_MUTEX_ATTR_MAX_SIZE 40
typedef union musl_pthread_mutexattr_t {
  uint8_t mutex_buffer[MUSL_MUTEX_ATTR_MAX_SIZE];
  void* ptr;
} musl_pthread_mutexattr_t;

SB_EXPORT int __wrap_pthread_mutex_destroy(musl_pthread_mutex_t* mutex);
SB_EXPORT int __wrap_pthread_mutex_init(
    musl_pthread_mutex_t* mutex,
    const musl_pthread_mutexattr_t* mutex_attr);
SB_EXPORT int __wrap_pthread_mutex_lock(musl_pthread_mutex_t* mutex);
SB_EXPORT int __wrap_pthread_mutex_unlock(musl_pthread_mutex_t* mutex);
SB_EXPORT int __wrap_pthread_mutex_trylock(musl_pthread_mutex_t* mutex);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_POSIX_PTHREAD_WRAPPERS_H_
