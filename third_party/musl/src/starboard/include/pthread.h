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

#ifndef THIRD_PARTY_MUSL_SRC_STARBOARD_INCLUDE_PTHREAD_H_
#define THIRD_PARTY_MUSL_SRC_STARBOARD_INCLUDE_PTHREAD_H_

#include <stdint.h>
#include <time.h>

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_DEFAULT 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_PRIO_INHERIT 1

#define PTHREAD_SCOPE_SYSTEM 0
#define PTHREAD_SCOPE_PROCESS 1

#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED 1

#define PTHREAD_RWLOCK_INITIALIZER {{0}}

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

// We use a non-zero value in recursive_flag to indicate that the
// mutex should be initialized as recursive on first use. This relies on the
// fact that PosixMutexPrivate has initialized_state as its last member.
// The recursive_flag overlaps with the beginning of mutex_buffer.
// This is safe because the flag is only read during the first-use
// initialization. Once initialized, the platform mutex handles the
// memory at this offset, and the flag is no longer needed.
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP \
  { {1} }

// Max size of the native mutex type.
#define MUSL_PTHREAD_MUTEX_MAX_SIZE 80

// An opaque handle to a native mutex type with reserved memory
// buffer aligned at void  pointer type.
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
// buffer aligned at void  pointer type.
typedef union pthread_mutexattr_t {
  // Reserved memory in which the implementation should map its
  // native mutex attribute type.
  uint8_t mutex_buffer[MUSL_PTHREAD_MUTEX_ATTR_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_mutexattr_t;

typedef uintptr_t pthread_key_t;

#ifdef __cplusplus
#define PTHREAD_COND_INITIALIZER \
  {}
#else
#define PTHREAD_COND_INITIALIZER \
  { 0 }
#endif

// Max size of the native conditional variable type.
#define MUSL_PTHREAD_COND_MAX_SIZE 96

// An opaque handle to a native conditional type with reserved memory
// buffer aligned at void  pointer type.
typedef union pthread_cond_t {
  // Reserved memory in which the implementation should map its
  // native conditional type.
  uint8_t cond_buffer[MUSL_PTHREAD_COND_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_cond_t;

// Max size of the native conditional attribute type.
#define MUSL_PTHREAD_COND_ATTR_MAX_SIZE 40

// An opaque handle to a native conditional attribute type with reserved memory
// buffer aligned at void  pointer type.
typedef union pthread_condattr_t {
  // Reserved memory in which the implementation should map its
  // native conditional attribute type.
  uint8_t cond_buffer[MUSL_PTHREAD_COND_ATTR_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_condattr_t;

// Max size of the native conditional attribute type.
#define MUSL_PTHREAD_ATTR_MAX_SIZE 80

// An opaque handle to a native attribute type with reserved memory
// buffer aligned at void  pointer type.
typedef union pthread_attr_t {
  // Reserved memory in which the implementation should map its
  // native attribute type.
  uint8_t attr_buffer[MUSL_PTHREAD_ATTR_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_attr_t;

// Max size of the native rwlock type.
#define MUSL_PTHREAD_RWLOCK_MAX_SIZE 80

// An opaque handle to a native attribute type with reserved memory
// buffer aligned at void  pointer type.
typedef union pthread_rwlock_t {
  // Reserved memory in which the implementation should map its
  // native rwlock type.
  uint8_t rwlock_buffer[MUSL_PTHREAD_RWLOCK_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_rwlock_t;

// Max size of the native conditional attribute type.
#define MUSL_PTHREAD_RWLOCK_ATTR_MAX_SIZE 8

// An opaque handle to a native attribute type with reserved memory
// buffer aligned at void  pointer type.
typedef union pthread_rwlockattr_t {
  // Reserved memory in which the implementation should map its
  // native attribute type.
  uint8_t rwlock_buffer[MUSL_PTHREAD_RWLOCK_ATTR_MAX_SIZE];
} pthread_rwlockattr_t;

typedef int clockid_t;
typedef uintptr_t pthread_t;

// Max size of the pthread_once_t type.
#define MUSL_PTHREAD_ONCE_MAX_SIZE 64

// An opaque handle to a once control type with
// aligned at void  pointer type.
typedef union pthread_once_t {
  // Reserved memory in which the implementation should map its
  // native once control variable type.
  uint8_t once_buffer[MUSL_PTHREAD_ONCE_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} pthread_once_t;

#ifdef __cplusplus
#define PTHREAD_ONCE_INIT \
  {}
#else
#define PTHREAD_ONCE_INIT \
  { 0 }
#endif

int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_init(pthread_mutex_t* mutex,
                       const pthread_mutexattr_t* mutex_attr);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
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

int pthread_setname_np(pthread_t thread, const char* name);
int pthread_getname_np(pthread_t thread, char* name, size_t len);

int pthread_attr_destroy(pthread_attr_t* attr);
int pthread_attr_init(pthread_attr_t* attr);

int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stack_size);
int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stack_size);

int pthread_attr_getdetachstate(const pthread_attr_t* att, int* detach_state);
int pthread_attr_setdetachstate(pthread_attr_t* attr, int detach_state);

// TODO: b/399696581 - Cobalt: Implement pthread API's
int pthread_getaffinity_np(pthread_t, size_t, struct cpu_set_t*);
int pthread_setaffinity_np(pthread_t, size_t, const struct cpu_set_t*);

int pthread_attr_getscope(const pthread_attr_t* __restrict, int* __restrict);
int pthread_attr_setscope(pthread_attr_t*, int);
int pthread_attr_getschedpolicy(const pthread_attr_t* __restrict,
                                int* __restrict);
int pthread_attr_setschedpolicy(pthread_attr_t*, int);
int pthread_getattr_np(pthread_t, pthread_attr_t*);
int pthread_attr_getstack(const pthread_attr_t* __restrict,
                          void** __restrict,
                          size_t* __restrict);
int pthread_attr_setstack(pthread_attr_t*, void*, size_t);
int pthread_mutexattr_init(pthread_mutexattr_t*);
int pthread_mutexattr_destroy(pthread_mutexattr_t*);
int pthread_mutexattr_gettype(const pthread_mutexattr_t* __restrict,
                              int* __restrict);
int pthread_mutexattr_settype(pthread_mutexattr_t*, int);
int pthread_mutexattr_getpshared(const pthread_mutexattr_t* __restrict,
                                 int* __restrict);
int pthread_mutexattr_setpshared(pthread_mutexattr_t*, int);
int pthread_atfork(void (*)(void), void (*)(void), void (*)(void));
int pthread_mutexattr_setprotocol(pthread_mutexattr_t*, int);
int pthread_getschedparam(pthread_t,
                          int* __restrict,
                          struct sched_param* __restrict);
int pthread_setschedparam(pthread_t, int, const struct sched_param*);

int pthread_rwlock_init(pthread_rwlock_t* __restrict,
                        const pthread_rwlockattr_t* __restrict);
int pthread_rwlock_destroy(pthread_rwlock_t*);
int pthread_rwlock_rdlock(pthread_rwlock_t*);
int pthread_rwlock_wrlock(pthread_rwlock_t*);
int pthread_rwlock_unlock(pthread_rwlock_t*);
int pthread_rwlock_tryrdlock(pthread_rwlock_t*);
int pthread_rwlock_trywrlock(pthread_rwlock_t*);

int pthread_kill(pthread_t, int);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_INCLUDE_PTHREAD_H_
