// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Mutex module
//
// Defines a mutually exclusive lock that can be used to coordinate with other
// threads.

#ifndef STARBOARD_MUTEX_H_
#define STARBOARD_MUTEX_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/thread.h"
#include "starboard/thread_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SB_API_VERSION >= 12

// Max size of the SbMutex type.
#define SB_MUTEX_MAX_SIZE 80

// An opaque handle to a mutex type with reserved memory
// buffer of size SB_MUTEX_MAX_SIZE and aligned at void
// pointer type.
typedef union SbMutex {
  // Reserved memory in which the implementation should map its
  // native mutex type.
  uint8_t mutex_buffer[SB_MUTEX_MAX_SIZE];

  // Guarantees alignment of the type to a void pointer.
  void* ptr;
} SbMutex;

#ifdef __cplusplus
#define SB_MUTEX_INITIALIZER \
  {}
#else
#define SB_MUTEX_INITIALIZER \
  { 0 }
#endif

#endif  // SB_API_VERSION >= 12

// Enumeration of possible results from acquiring a mutex.
typedef enum SbMutexResult {
  // The mutex was acquired successfully.
  kSbMutexAcquired,

  // The mutex was not acquired because it was held by someone else.
  kSbMutexBusy,

  // The mutex has already been destroyed.
  kSbMutexDestroyed,
} SbMutexResult;

// Indicates whether the given result is a success. A value of |true| indicates
// that the mutex was acquired.
//
// |result|: The result being checked.
static SB_C_FORCE_INLINE bool SbMutexIsSuccess(SbMutexResult result) {
  return result == kSbMutexAcquired;
}

// Creates a new mutex. The return value indicates whether the function
// was able to create a new mutex.
//
// |out_mutex|: The handle to the newly created mutex.
SB_EXPORT bool SbMutexCreate(SbMutex* out_mutex);

#if SB_API_VERSION >= 12
// Destroys a mutex. The return value indicates whether the destruction was
// successful. Destroying a locked mutex results in undefined behavior.
//
// |mutex|: The mutex to be invalidated.
#else   // SB_API_VERSION >= 12
// Destroys a mutex. The return value indicates whether the destruction was
// successful.
//
// |mutex|: The mutex to be invalidated.
#endif  // SB_API_VERSION >= 12
SB_EXPORT bool SbMutexDestroy(SbMutex* mutex);

// Acquires |mutex|, blocking indefinitely. The return value identifies
// the acquisition result. SbMutexes are not reentrant, so a recursive
// acquisition blocks forever.
//
// |mutex|: The mutex to be acquired.
SB_EXPORT SbMutexResult SbMutexAcquire(SbMutex* mutex);

#if SB_API_VERSION >= 12
// Acquires |mutex|, without blocking. The return value identifies
// the acquisition result. SbMutexes are not reentrant, so a recursive
// acquisition has undefined behavior.
//
// |mutex|: The mutex to be acquired.
#else   // SB_API_VERSION >= 12
// Acquires |mutex|, without blocking. The return value identifies
// the acquisition result. SbMutexes are not reentrant, so a recursive
// acquisition always fails.
//
// |mutex|: The mutex to be acquired.
#endif  // SB_API_VERSION >= 12
SB_EXPORT SbMutexResult SbMutexAcquireTry(SbMutex* mutex);

// Releases |mutex| held by the current thread. The return value indicates
// whether the release was successful. Releases should always be successful
// if |mutex| is held by the current thread.
//
// |mutex|: The mutex to be released.
SB_EXPORT bool SbMutexRelease(SbMutex* mutex);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_MUTEX_H_
