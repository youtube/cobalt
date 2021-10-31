// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/lazy_initialization_public.h"

#ifndef STARBOARD_SHARED_PTHREAD_TYPES_INTERNAL_H_
#define STARBOARD_SHARED_PTHREAD_TYPES_INTERNAL_H_

#if SB_API_VERSION >= 12
#define SB_INTERNAL_MUTEX(mutex_var) \
  reinterpret_cast<SbMutexPrivate*>((mutex_var)->mutex_buffer)
#define SB_PTHREAD_INTERNAL_MUTEX(mutex_var) \
  &(reinterpret_cast<SbMutexPrivate*>((mutex_var)->mutex_buffer)->mutex)
#define SB_INTERNAL_ONCE(once_var) \
  reinterpret_cast<SbOnceControlPrivate*>((once_var)->once_buffer)
#define SB_PTHREAD_INTERNAL_ONCE(once_var) \
  &(reinterpret_cast<SbOnceControlPrivate*>((once_var)->once_buffer)->once)
#define SB_PTHREAD_INTERNAL_THREAD(thread) reinterpret_cast<pthread_t>(thread)
#define SB_PTHREAD_INTERNAL_THREAD_PTR(thread) \
  reinterpret_cast<pthread_t*>(&(thread))
#define SB_THREAD(thread) reinterpret_cast<SbThread>(thread)
#define SB_PTHREAD_INTERNAL_CONDITION(condition_var) \
  reinterpret_cast<SbConditionVariablePrivate*>(     \
      (condition_var)->condition_buffer)
#else
#define SB_PTHREAD_INTERNAL_MUTEX(mutex_var) (mutex_var)
#define SB_PTHREAD_INTERNAL_ONCE(once_var) (once_var)
#define SB_PTHREAD_INTERNAL_THREAD(thread) (thread)
#define SB_PTHREAD_INTERNAL_THREAD_PTR(thread) \
  reinterpret_cast<pthread_t*>(&(thread))
#define SB_THREAD(thread) (thread)
#define SB_PTHREAD_INTERNAL_CONDITION(condition_var) (condition_var)
#endif  // SB_API_VERSION >= 12

// Transparent Condition Variable handle.
// It is customized from the plain pthread_cont_t object because we
// need to ensure that each condition variable is initialized to use
// CLOCK_MONOTONIC, which is not the default (and the default is used
// when PTHREAD_COND_INITIALIZER is set).
typedef struct SbConditionVariablePrivate {
  InitializedState initialized_state;
  pthread_cond_t condition;
} SbConditionVariablePrivate;

// Wrapping pthread_mutex_t to add initialization state
// which allows for lazy initialization to PTHREAD_MUTEX_INITIALIZER.
// NOTE: The actual value of PTHREAD_MUTEX_INITIALIZER.
typedef struct SbMutexPrivate {
  InitializedState initialized_state;
  pthread_mutex_t mutex;
} SbMutexPrivate;

// Wrapping pthread_once_t to add initialization state
// which allows for lazy initialization to PTHREAD_ONCE_INIT.
// NOTE: The actual value of PTHREAD_ONCE_INIT.
typedef struct SbOnceControlPrivate {
  InitializedState initialized_state;
  pthread_once_t once;
} SbOnceControlPrivate;

#endif  // STARBOARD_SHARED_PTHREAD_TYPES_INTERNAL_H_
