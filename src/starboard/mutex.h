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

// Module Overview: Starboard Mutex module
//
// Defines a mutually exclusive lock that can be used to coordinate with other
// threads.

#ifndef STARBOARD_MUTEX_H_
#define STARBOARD_MUTEX_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/log.h"
#include "starboard/thread.h"
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

// Destroys a mutex. The return value indicates whether the destruction was
// successful.
//
// |mutex|: The mutex to be invalidated.
SB_EXPORT bool SbMutexDestroy(SbMutex* mutex);

// Acquires |mutex|, blocking indefinitely. The return value identifies
// the acquisition result. SbMutexes are not reentrant, so a recursive
// acquisition blocks forever.
//
// |mutex|: The mutex to be acquired.
SB_EXPORT SbMutexResult SbMutexAcquire(SbMutex* mutex);

// Acquires |mutex|, without blocking. The return value identifies
// the acquisition result. SbMutexes are not reentrant, so a recursive
// acquisition always fails.
//
// |mutex|: The mutex to be acquired.
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

#ifdef __cplusplus
namespace starboard {

// Inline class wrapper for SbMutex.
class Mutex {
 public:
  Mutex() : mutex_() {
    SbMutexCreate(&mutex_);
    debugInit();
  }

  ~Mutex() { SbMutexDestroy(&mutex_); }

  void Acquire() const {
    SbMutexAcquire(&mutex_);
    debugSetAcquired();
  }
  bool AcquireTry() const {
    bool ok = SbMutexAcquireTry(&mutex_) == kSbMutexAcquired;
    if (ok) {
      debugSetAcquired();
    }
    return ok;
  }

  void Release() const {
    debugSetReleased();
    SbMutexRelease(&mutex_);
  }

  void DCheckAcquired() const {
#ifdef _DEBUG
    SB_DCHECK(currrent_thread_acquired_ == SbThreadGetCurrent());
#endif  // _DEBUG
  }

 private:
#ifdef _DEBUG
  void debugInit() { currrent_thread_acquired_ = kSbThreadInvalid; }
  void debugSetReleased() const {
    SbThread current_thread = SbThreadGetCurrent();
    SB_DCHECK(currrent_thread_acquired_ == current_thread);
    currrent_thread_acquired_ = kSbThreadInvalid;
  }
  void debugSetAcquired() const {
    // Check that the thread has already not been held.
    SB_DCHECK(currrent_thread_acquired_ == kSbThreadInvalid);
    currrent_thread_acquired_ = SbThreadGetCurrent();
  }
  mutable SbThread currrent_thread_acquired_;
#else
  void debugInit() {}
  void debugSetReleased() const {}
  void debugSetAcquired() const {}
#endif

  friend class ConditionVariable;
  SbMutex* mutex() const { return &mutex_; }
  mutable SbMutex mutex_;

  SB_DISALLOW_COPY_AND_ASSIGN(Mutex);
};

// Scoped lock holder that works on starboard::Mutex.
class ScopedLock {
 public:
  explicit ScopedLock(const Mutex& mutex) : mutex_(mutex) { mutex_.Acquire(); }

  ~ScopedLock() { mutex_.Release(); }

 private:
  const Mutex& mutex_;
  SB_DISALLOW_COPY_AND_ASSIGN(ScopedLock);
};

// Scoped lock holder that works on starboard::Mutex which uses AcquireTry()
// instead of Acquire().
class ScopedTryLock {
 public:
  explicit ScopedTryLock(const Mutex& mutex) : mutex_(mutex) {
    is_locked_ = mutex_.AcquireTry();
  }

  ~ScopedTryLock() {
    if (is_locked_) {
      mutex_.Release();
    }
  }

  bool is_locked() const { return is_locked_; }

 private:
  const Mutex& mutex_;
  bool is_locked_;
  SB_DISALLOW_COPY_AND_ASSIGN(ScopedTryLock);
};

}  // namespace starboard
#endif

#endif  // STARBOARD_MUTEX_H_
