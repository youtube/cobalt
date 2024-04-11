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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_PTHREAD_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_PTHREAD_H_

#include <time.h>
#include <windows.h>

#include "starboard/shared/win32/posix_emu/include/remove_problematic_windows_macros.h"

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
#define PTHREAD_COND_INITIALIZER CONDITION_VARIABLE_INIT
#define PTHREAD_ONCE_INIT INIT_ONCE_STATIC_INIT

#ifdef __cplusplus
extern "C" {
#endif

typedef SRWLOCK pthread_mutex_t;
typedef unsigned int pthread_mutexattr_t;
typedef DWORD pthread_key_t;
typedef CONDITION_VARIABLE pthread_cond_t;
typedef unsigned int pthread_condattr_t;
typedef INIT_ONCE pthread_once_t;
typedef void* pthread_t;
typedef void* pthread_attr_t;

int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_init(pthread_mutex_t* mutex,
                       const pthread_mutexattr_t* mutex_attr);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);

int pthread_key_create(pthread_key_t* key, void (*)(void* dtor));
int pthread_key_delete(pthread_key_t key);
void* pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void* value);

int pthread_cond_broadcast(pthread_cond_t* cond);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_timedwait(pthread_cond_t* cond,
                           pthread_mutex_t* mutex,
                           const struct timespec* t);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_condattr_destroy(pthread_condattr_t* attr);
int pthread_condattr_getclock(const pthread_condattr_t* attr,
                              clockid_t* clock_id);
int pthread_condattr_init(pthread_condattr_t* attr);
int pthread_condattr_setclock(pthread_condattr_t* attr, clockid_t clock_id);

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void));

int pthread_create(pthread_t* thread,
                   const pthread_attr_t* attr,
                   void* (*start_routine)(void*),
                   void* arg);
int pthread_join(pthread_t thread, void** value_ptr);
int pthread_detach(pthread_t thread);
pthread_t pthread_self();
int pthread_equal(pthread_t t1, pthread_t t2);

#ifdef __cplusplus
}
#endif

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_PTHREAD_H_
