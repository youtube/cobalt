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

#include <semaphore.h>
#include <time.h>

extern "C" {

// ABI wrapper for sem_destroy. This function is the actual implementation
// that will be called when sem_destroy is invoked.
int __abi_wrap_sem_destroy(sem_t* sem);

// The standard sem_destroy function, which is redirected to the ABI wrapper.
int sem_destroy(sem_t* sem) {
  return __abi_wrap_sem_destroy(sem);
}

// ABI wrapper for sem_init.
int __abi_wrap_sem_init(sem_t* sem, int pshared, unsigned int value);

// The standard sem_init function, redirected to the wrapper.
int sem_init(sem_t* sem, int pshared, unsigned int value) {
  return __abi_wrap_sem_init(sem, pshared, value);
}

// ABI wrapper for sem_post.
int __abi_wrap_sem_post(sem_t* sem);

// The standard sem_post function, redirected to the wrapper.
int sem_post(sem_t* sem) {
  return __abi_wrap_sem_post(sem);
}

// ABI wrapper for sem_timedwait.
int __abi_wrap_sem_timedwait(sem_t* sem, const struct timespec* abs_timeout);

// The standard sem_timedwait function, redirected to the wrapper.
int sem_timedwait(sem_t* sem, const struct timespec* abs_timeout) {
  return __abi_wrap_sem_timedwait(sem, abs_timeout);
}

// ABI wrapper for sem_wait.
int __abi_wrap_sem_wait(sem_t* sem);

// The standard sem_wait function, redirected to the wrapper.
int sem_wait(sem_t* sem) {
  return __abi_wrap_sem_wait(sem);
}

}  // extern "C"
