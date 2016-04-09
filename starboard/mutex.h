// Copyright 2015 Google Inc. All Rights Reserved.
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

// A mutually exclusive lock that can be used to coordinate with other threads.

#ifndef STARBOARD_MUTEX_H_
#define STARBOARD_MUTEX_H_

#include "starboard/export.h"
#include "starboard/thread_types.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Enumeration of possible results from acquiring a mutex.
typedef enum SbMutexResult {
  // The mutex was acquired successfully.
  kSbMutexAcquired,

  // The mutex was not acquired because it was held by someone else.
  kSbMutexBusy,

  // The mutex has already been destroyed.
  kSbMutexDestroyed,
} SbMutexResult;

// Returns whether the given result is a success.
static SB_C_FORCE_INLINE bool SbMutexIsSuccess(SbMutexResult result) {
  return result == kSbMutexAcquired;
}

// Creates a new mutex, placing the handle to the newly created mutex in
// |out_mutex|. Returns whether able to create a new mutex.
SB_EXPORT bool SbMutexCreate(SbMutex* out_mutex);

// Destroys a mutex, returning whether the destruction was successful. The mutex
// specified by |mutex| is invalidated.
SB_EXPORT bool SbMutexDestroy(SbMutex* mutex);

// Acquires |mutex|, blocking indefinitely, returning the acquisition result.
// SbMutexes are not reentrant, so a recursive acquisition will block forever.
SB_EXPORT SbMutexResult SbMutexAcquire(SbMutex* mutex);

// Acquires |mutex|, without blocking, returning the acquisition result.
// SbMutexes are not reentrant, so a recursive acquisition will always fail.
SB_EXPORT SbMutexResult SbMutexAcquireTry(SbMutex* mutex);

// Releases |mutex| held by the current thread, returning whether the release
// was successful. Releases should always be successful if the mutex is held by
// the current thread.
SB_EXPORT bool SbMutexRelease(SbMutex* handle);

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __cplusplus
namespace starboard {

// Inline class wrapper for SbMutex.
class Mutex {
 public:
  Mutex() : mutex_() { SbMutexCreate(&mutex_); }

  ~Mutex() { SbMutexDestroy(&mutex_); }

  void Acquire() const { SbMutexAcquire(&mutex_); }
  bool AcquireTry() { return SbMutexAcquireTry(&mutex_) == kSbMutexAcquired; }

  void Release() const { SbMutexRelease(&mutex_); }

 private:
  friend class ConditionVariable;
  SbMutex* mutex() const { return &mutex_; }

  mutable SbMutex mutex_;
};

// Scoped lock holder that works on starboard::Mutex.
class ScopedLock {
 public:
  explicit ScopedLock(const Mutex& mutex) : mutex_(mutex) { mutex_.Acquire(); }

  ~ScopedLock() { mutex_.Release(); }

  const Mutex& mutex_;
};

}  // namespace starboard
#endif

#endif  // STARBOARD_MUTEX_H_
