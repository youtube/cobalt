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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_PTHREAD_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_PTHREAD_ABI_WRAPPERS_H_

#include <stdint.h>

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/shared/modular/starboard_layer_posix_time_abi_wrappers.h"

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

#define MUSL_PTHREAD_COND_MAX_SIZE 80
typedef union musl_pthread_cond_t {
  uint8_t cond_buffer[MUSL_PTHREAD_COND_MAX_SIZE];
  void* ptr;
} musl_pthread_cond_t;

#define MUSL_PTHREAD_COND_ATTR_MAX_SIZE 40
typedef union musl_pthread_condattr_t {
  uint8_t cond_attr_buffer[MUSL_PTHREAD_COND_ATTR_MAX_SIZE];
  void* ptr;
} musl_pthread_condattr_t;

#define MUSL_PTHREAD_ATTR_MAX_SIZE 80
typedef union musl_pthread_attr_t {
  uint8_t attr_buffer[MUSL_PTHREAD_ATTR_MAX_SIZE];
  void* ptr;
} musl_pthread_attr_t;

#define MUSL_PTHREAD_ONCE_MAX_SIZE 64
typedef union musl_pthread_once_t {
  uint8_t once_buffer[MUSL_PTHREAD_ONCE_MAX_SIZE];
  void* ptr;
} musl_pthread_once_t;

typedef void* musl_pthread_t;
typedef void* musl_pthread_key_t;

SB_EXPORT int __abi_wrap_pthread_mutex_destroy(musl_pthread_mutex_t* mutex);
SB_EXPORT int __abi_wrap_pthread_mutex_init(
    musl_pthread_mutex_t* mutex,
    const musl_pthread_mutexattr_t* mutex_attr);
SB_EXPORT int __abi_wrap_pthread_mutex_lock(musl_pthread_mutex_t* mutex);
SB_EXPORT int __abi_wrap_pthread_mutex_unlock(musl_pthread_mutex_t* mutex);
SB_EXPORT int __abi_wrap_pthread_mutex_trylock(musl_pthread_mutex_t* mutex);

SB_EXPORT int __abi_wrap_pthread_cond_broadcast(musl_pthread_cond_t* cond);
SB_EXPORT int __abi_wrap_pthread_cond_destroy(musl_pthread_cond_t* cond);
SB_EXPORT int __abi_wrap_pthread_cond_init(musl_pthread_cond_t* cond,
                                           const musl_pthread_condattr_t* attr);
SB_EXPORT int __abi_wrap_pthread_cond_signal(musl_pthread_cond_t* cond);
SB_EXPORT int __abi_wrap_pthread_cond_timedwait(musl_pthread_cond_t* cond,
                                                musl_pthread_mutex_t* mutex,
                                                const struct musl_timespec* t);
SB_EXPORT int __abi_wrap_pthread_cond_wait(musl_pthread_cond_t* cond,
                                           musl_pthread_mutex_t* mutex);
SB_EXPORT int __abi_wrap_pthread_condattr_destroy(
    musl_pthread_condattr_t* attr);
SB_EXPORT int __abi_wrap_pthread_condattr_getclock(
    const musl_pthread_condattr_t* attr,
    clockid_t* clock_id);
SB_EXPORT int __abi_wrap_pthread_condattr_init(musl_pthread_condattr_t* attr);
SB_EXPORT int __abi_wrap_pthread_condattr_setclock(
    musl_pthread_condattr_t* attr,
    clockid_t clock_id);

SB_EXPORT int __abi_wrap_pthread_once(musl_pthread_once_t* once_control,
                                      void (*init_routine)(void));

SB_EXPORT int __abi_wrap_pthread_create(musl_pthread_t* thread,
                                        const musl_pthread_attr_t* attr,
                                        void* (*start_routine)(void*),
                                        void* arg);
SB_EXPORT int __abi_wrap_pthread_join(musl_pthread_t thread, void** value_ptr);
SB_EXPORT int __abi_wrap_pthread_detach(musl_pthread_t thread);
SB_EXPORT int __abi_wrap_pthread_equal(musl_pthread_t t1, musl_pthread_t t2);
SB_EXPORT musl_pthread_t __abi_wrap_pthread_self();
SB_EXPORT int __abi_wrap_pthread_key_create(musl_pthread_key_t* key,
                                            void (*destructor)(void*));
SB_EXPORT int __abi_wrap_pthread_key_delete(musl_pthread_key_t key);
SB_EXPORT void* __abi_wrap_pthread_getspecific(musl_pthread_key_t key);
SB_EXPORT int __abi_wrap_pthread_setspecific(musl_pthread_key_t key,
                                             const void* value);

SB_EXPORT int __abi_wrap_pthread_setname_np(musl_pthread_t thread,
                                            const char* name);
SB_EXPORT int __abi_wrap_pthread_getname_np(musl_pthread_t thread,
                                            char* name,
                                            size_t len);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_PTHREAD_ABI_WRAPPERS_H_
