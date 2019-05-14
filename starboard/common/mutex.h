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

// Defines convenience classes that wrap the core Starboard mutually exclusive
// locking API to provide additional locking mechanisms.

#ifndef STARBOARD_COMMON_MUTEX_H_
#define STARBOARD_COMMON_MUTEX_H_

#include "starboard/common/log.h"
#include "starboard/mutex.h"
#include "starboard/thread.h"
#include "starboard/thread_types.h"

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
    debugPreAcquire();
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
  void debugPreAcquire() const {
    // Check that the mutex is not held by the current thread.
    SbThread current_thread = SbThreadGetCurrent();
    SB_DCHECK(currrent_thread_acquired_ != current_thread);
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
  void debugPreAcquire() const {}
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

#endif  // STARBOARD_COMMON_MUTEX_H_
