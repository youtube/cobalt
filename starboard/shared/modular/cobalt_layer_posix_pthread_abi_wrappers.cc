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

#include <pthread.h>

extern "C" {

int __abi_wrap_pthread_mutex_destroy(pthread_mutex_t* mutex);

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
  return __abi_wrap_pthread_mutex_destroy(mutex);
}

int __abi_wrap_pthread_mutex_init(pthread_mutex_t* mutex,
                                  const pthread_mutexattr_t* attr);

int pthread_mutex_init(pthread_mutex_t* mutex,
                       const pthread_mutexattr_t* attr) {
  return __abi_wrap_pthread_mutex_init(mutex, attr);
}

int __abi_wrap_pthread_mutex_lock(pthread_mutex_t* mutex);

int pthread_mutex_lock(pthread_mutex_t* mutex) {
  return __abi_wrap_pthread_mutex_lock(mutex);
}

int __abi_wrap_pthread_mutex_unlock(pthread_mutex_t* mutex);

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
  return __abi_wrap_pthread_mutex_unlock(mutex);
}

int __abi_wrap_pthread_mutex_trylock(pthread_mutex_t* mutex);

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
  return __abi_wrap_pthread_mutex_trylock(mutex);
}

int __abi_wrap_pthread_cond_broadcast(pthread_cond_t* cond);

int pthread_cond_broadcast(pthread_cond_t* cond) {
  return __abi_wrap_pthread_cond_broadcast(cond);
}

int __abi_wrap_pthread_cond_destroy(pthread_cond_t* cond);

int pthread_cond_destroy(pthread_cond_t* cond) {
  return __abi_wrap_pthread_cond_destroy(cond);
}

int __abi_wrap_pthread_cond_init(pthread_cond_t* cond,
                                 const pthread_condattr_t* attr);

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
  return __abi_wrap_pthread_cond_init(cond, attr);
}

int __abi_wrap_pthread_cond_signal(pthread_cond_t* cond);

int pthread_cond_signal(pthread_cond_t* cond) {
  return __abi_wrap_pthread_cond_signal(cond);
}

int __abi_wrap_pthread_cond_timedwait(pthread_cond_t* cond,
                                      pthread_mutex_t* mutex,
                                      const struct timespec* t);

int pthread_cond_timedwait(pthread_cond_t* cond,
                           pthread_mutex_t* mutex,
                           const struct timespec* t) {
  return __abi_wrap_pthread_cond_timedwait(cond, mutex, t);
}

int __abi_wrap_pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
  return __abi_wrap_pthread_cond_wait(cond, mutex);
}

int __abi_wrap_pthread_condattr_destroy(pthread_condattr_t* attr);

int pthread_condattr_destroy(pthread_condattr_t* attr) {
  return __abi_wrap_pthread_condattr_destroy(attr);
}

int __abi_wrap_pthread_condattr_getclock(const pthread_condattr_t* attr,
                                         clockid_t* clock_id);

int pthread_condattr_getclock(const pthread_condattr_t* attr,
                              clockid_t* clock_id) {
  return __abi_wrap_pthread_condattr_getclock(attr, clock_id);
}

int __abi_wrap_pthread_condattr_init(pthread_condattr_t* attr);

int pthread_condattr_init(pthread_condattr_t* attr) {
  return __abi_wrap_pthread_condattr_init(attr);
}

int __abi_wrap_pthread_condattr_setclock(pthread_condattr_t* attr,
                                         clockid_t clock_id);

int pthread_condattr_setclock(pthread_condattr_t* attr, clockid_t clock_id) {
  return __abi_wrap_pthread_condattr_setclock(attr, clock_id);
}

int __abi_wrap_pthread_once(pthread_once_t* once_control,
                            void (*init_routine)(void));

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void)) {
  return __abi_wrap_pthread_once(once_control, init_routine);
}

int __abi_wrap_pthread_create(pthread_t* thread,
                              const pthread_attr_t* attr,
                              void* (*start_routine)(void*),
                              void* arg);

int pthread_create(pthread_t* thread,
                   const pthread_attr_t* attr,
                   void* (*start_routine)(void*),
                   void* arg) {
  return __abi_wrap_pthread_create(thread, attr, start_routine, arg);
}

int __abi_wrap_pthread_join(pthread_t thread, void** value_ptr);

int pthread_join(pthread_t thread, void** value_ptr) {
  return __abi_wrap_pthread_join(thread, value_ptr);
}

int __abi_wrap_pthread_detach(pthread_t thread);

int pthread_detach(pthread_t thread) {
  return __abi_wrap_pthread_detach(thread);
}

int __abi_wrap_pthread_equal(pthread_t t1, pthread_t t2);

int pthread_equal(pthread_t t1, pthread_t t2) {
  return __abi_wrap_pthread_equal(t1, t2);
}

pthread_t __abi_wrap_pthread_self();

pthread_t pthread_self() {
  return __abi_wrap_pthread_self();
}

int __abi_wrap_pthread_key_create(pthread_key_t* key,
                                  void (*destructor)(void*));

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*)) {
  return __abi_wrap_pthread_key_create(key, destructor);
}

int __abi_wrap_pthread_key_delete(pthread_key_t key);

int pthread_key_delete(pthread_key_t key) {
  return __abi_wrap_pthread_key_delete(key);
}

void* __abi_wrap_pthread_getspecific(pthread_key_t key);

void* pthread_getspecific(pthread_key_t key) {
  return __abi_wrap_pthread_getspecific(key);
}

int __abi_wrap_pthread_setspecific(pthread_key_t key, const void* value);

int pthread_setspecific(pthread_key_t key, const void* value) {
  return __abi_wrap_pthread_setspecific(key, value);
}

int __abi_wrap_pthread_setname_np(pthread_t thread, const char* name);

int pthread_setname_np(pthread_t thread, const char* name) {
  return __abi_wrap_pthread_setname_np(thread, name);
}

int __abi_wrap_pthread_getname_np(pthread_t thread, char* name, size_t len);

int pthread_getname_np(pthread_t thread, char* name, size_t len) {
  return __abi_wrap_pthread_getname_np(thread, name, len);
}

int __abi_wrap_pthread_getattr_np(pthread_t thread, pthread_attr_t* attr);
int pthread_getattr_np(pthread_t thread, pthread_attr_t* attr) {
  return __abi_wrap_pthread_getattr_np(thread, attr);
}

int __abi_wrap_pthread_attr_destroy(pthread_attr_t* attr);

int pthread_attr_destroy(pthread_attr_t* attr) {
  return __abi_wrap_pthread_attr_destroy(attr);
}

int __abi_wrap_pthread_attr_init(pthread_attr_t* attr);

int pthread_attr_init(pthread_attr_t* attr) {
  return __abi_wrap_pthread_attr_init(attr);
}

int __abi_wrap_pthread_attr_getscope(const pthread_attr_t* __restrict attr,
                                     int* __restrict scope);
int pthread_attr_getscope(const pthread_attr_t* __restrict attr,
                          int* __restrict scope) {
  return __abi_wrap_pthread_attr_getscope(attr, scope);
}

int __abi_wrap_pthread_attr_setscope(pthread_attr_t* attr, int scope);
int pthread_attr_setscope(pthread_attr_t* attr, int scope) {
  return __abi_wrap_pthread_attr_setscope(attr, scope);
}

int __abi_wrap_pthread_attr_getschedpolicy(
    const pthread_attr_t* __restrict attr,
    int* __restrict policy);
int pthread_attr_getschedpolicy(const pthread_attr_t* __restrict attr,
                                int* __restrict policy) {
  return __abi_wrap_pthread_attr_getschedpolicy(attr, policy);
}

int __abi_wrap_pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy);
int pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy) {
  return __abi_wrap_pthread_attr_setschedpolicy(attr, policy);
}

int __abi_wrap_pthread_attr_getstacksize(const pthread_attr_t* attr,
                                         size_t* stack_size);

int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stack_size) {
  return __abi_wrap_pthread_attr_getstacksize(attr, stack_size);
}

int __abi_wrap_pthread_attr_getstack(const pthread_attr_t* __restrict attr,
                                     void** __restrict stackaddr,
                                     size_t* __restrict stacksize);

int pthread_attr_getstack(const pthread_attr_t* __restrict attr,
                          void** __restrict stackaddr,
                          size_t* __restrict stacksize) {
  return __abi_wrap_pthread_attr_getstack(attr, stackaddr, stacksize);
}

int __abi_wrap_pthread_attr_setstack(pthread_attr_t* attr,
                                     void* stackaddr,
                                     size_t stacksize);

int pthread_attr_setstack(pthread_attr_t* attr,
                          void* stackaddr,
                          size_t stacksize) {
  return __abi_wrap_pthread_attr_setstack(attr, stackaddr, stacksize);
}

int __abi_wrap_pthread_attr_setstacksize(pthread_attr_t* attr,
                                         size_t stack_size);

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stack_size) {
  return __abi_wrap_pthread_attr_setstacksize(attr, stack_size);
}

int __abi_wrap_pthread_attr_getdetachstate(const pthread_attr_t* att,
                                           int* detachs_state);

int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detach_state) {
  return __abi_wrap_pthread_attr_getdetachstate(attr, detach_state);
}

int __abi_wrap_pthread_attr_setdetachstate(pthread_attr_t* attr,
                                           int detach_state);

int pthread_attr_setdetachstate(pthread_attr_t* attr, int detach_state) {
  return __abi_wrap_pthread_attr_setdetachstate(attr, detach_state);
}

int __abi_wrap_pthread_getschedparam(pthread_t thread,
                                     int* __restrict policy,
                                     sched_param* __restrict param);
int pthread_getschedparam(pthread_t thread,
                          int* __restrict policy,
                          sched_param* __restrict param) {
  return __abi_wrap_pthread_getschedparam(thread, policy, param);
}

int __abi_wrap_pthread_setschedparam(pthread_t thread,
                                     int policy,
                                     const sched_param* __restrict param);
int pthread_setschedparam(pthread_t thread,
                          int policy,
                          const sched_param* __restrict param) {
  return __abi_wrap_pthread_setschedparam(thread, policy, param);
}

int __abi_wrap_pthread_mutexattr_init(pthread_mutexattr_t* attr);

int pthread_mutexattr_init(pthread_mutexattr_t* attr) {
  return __abi_wrap_pthread_mutexattr_init(attr);
}

int __abi_wrap_pthread_mutexattr_destroy(pthread_mutexattr_t* attr);

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr) {
  return __abi_wrap_pthread_mutexattr_destroy(attr);
}

int __abi_wrap_pthread_mutexattr_gettype(const pthread_mutexattr_t* __restrict,
                                         int* __restrict);

int pthread_mutexattr_gettype(const pthread_mutexattr_t* __restrict attr,
                              int* __restrict type) {
  return __abi_wrap_pthread_mutexattr_gettype(attr, type);
}

int __abi_wrap_pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);

int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type) {
  return __abi_wrap_pthread_mutexattr_settype(attr, type);
}

int __abi_wrap_pthread_mutexattr_getpshared(
    const pthread_mutexattr_t* __restrict,
    int* __restrict);

int pthread_mutexattr_getpshared(const pthread_mutexattr_t* __restrict attr,
                                 int* __restrict pshared) {
  return __abi_wrap_pthread_mutexattr_getpshared(attr, pshared);
}

int __abi_wrap_pthread_mutexattr_setpshared(pthread_mutexattr_t* attr,
                                            int pshared);

int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr, int pshared) {
  return __abi_wrap_pthread_mutexattr_setpshared(attr, pshared);
}

int __abi_wrap_pthread_rwlock_init(pthread_rwlock_t* __restrict,
                                   const pthread_rwlockattr_t* __restrict);
int pthread_rwlock_init(pthread_rwlock_t* __restrict rwlock,
                        const pthread_rwlockattr_t* __restrict attr) {
  return __abi_wrap_pthread_rwlock_init(rwlock, attr);
}

int __abi_wrap_pthread_rwlock_destroy(pthread_rwlock_t*);
int pthread_rwlock_destroy(pthread_rwlock_t* rwlock) {
  return __abi_wrap_pthread_rwlock_destroy(rwlock);
}

int __abi_wrap_pthread_rwlock_rdlock(pthread_rwlock_t*);
int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock) {
  return __abi_wrap_pthread_rwlock_rdlock(rwlock);
}

int __abi_wrap_pthread_rwlock_wrlock(pthread_rwlock_t*);
int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock) {
  return __abi_wrap_pthread_rwlock_wrlock(rwlock);
}

int __abi_wrap_pthread_rwlock_unlock(pthread_rwlock_t*);
int pthread_rwlock_unlock(pthread_rwlock_t* rwlock) {
  return __abi_wrap_pthread_rwlock_unlock(rwlock);
}

int __abi_wrap_pthread_rwlock_tryrdlock(pthread_rwlock_t*);
int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock) {
  return __abi_wrap_pthread_rwlock_tryrdlock(rwlock);
}

int __abi_wrap_pthread_rwlock_trywrlock(pthread_rwlock_t*);
int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock) {
  return __abi_wrap_pthread_rwlock_trywrlock(rwlock);
}
}
