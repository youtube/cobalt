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

#ifndef THIRD_PARTY_MUSL_SRC_STARBOARD_PTHREAD_H_
#define THIRD_PARTY_MUSL_SRC_STARBOARD_PTHREAD_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#define PTHREAD_MUTEX_INITIALIZER \
  {}
#else
#define PTHREAD_MUTEX_INITIALIZER \
  { 0 }
#endif

// Max size of the native mutex type.
#define MUSL_PTHREAD_MUTEX_MAX_SIZE 80

// An opaque handle to a native mutex type with reserved memory
// bufferaligned at void  pointer type.
typedef union pthread_mutex_t {
  // Reserved memory in which the implementation should map its
  // native mutex type.
  uint8_t mutex_buffer[MUSL_PTHREAD_MUTEX_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_mutex_t;


// Max size of the native mutex attribute type.
#define MUSL_PTHREAD_MUTEX_ATTR_MAX_SIZE 40

// An opaque handle to a native mutex attribute type with reserved memory
// bufferaligned at void  pointer type.
typedef union pthread_mutexattr_t {
  // Reserved memory in which the implementation should map its
  // native mutex attribute type.
  uint8_t mutex_buffer[MUSL_PTHREAD_MUTEX_ATTR_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_mutexattr_t;


// Max size of the native key type.
#define MUSL_PTHREAD_KEY_MAX_SIZE 40

// An opaque handle to a native key type with reserved memory
// bufferaligned at void  pointer type.
typedef union pthread_key_t {
  // Reserved memory in which the implementation should map its
  // native key type.
  uint8_t  pthread_key_t[MUSL_PTHREAD_KEY_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_key_t;

int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* mutex_attr);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);

int pthread_key_create(pthread_key_t* key, void (*)(void* dtor));
int pthread_key_delete(pthread_key_t key);
void *pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void* value);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_PTHREAD_H_
