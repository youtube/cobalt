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

// clang-format off
#include <windows.h>
#include <_remove_problematic_windows_macros.h>
// clang-format on

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT

#ifdef __cplusplus
extern "C" {
#endif

typedef SRWLOCK pthread_mutex_t;
typedef unsigned int pthread_mutexattr_t;
typedef DWORD pthread_key_t;

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

#ifdef __cplusplus
}
#endif

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_PTHREAD_H_
