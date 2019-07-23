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

#include "starboard/common/mutex.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace starboard {

Mutex::Mutex() : mutex_() {
  SbMutexCreate(&mutex_);
  debugInit();
}

Mutex::~Mutex() {
  SbMutexDestroy(&mutex_);
}

void Mutex::Acquire() const {
  debugPreAcquire();
  SbMutexAcquire(&mutex_);
  debugSetAcquired();
}
bool Mutex::AcquireTry() const {
  bool ok = SbMutexAcquireTry(&mutex_) == kSbMutexAcquired;
  if (ok) {
    debugSetAcquired();
  }
  return ok;
}

void Mutex::Release() const {
  debugSetReleased();
  SbMutexRelease(&mutex_);
}

void Mutex::DCheckAcquired() const {
#ifdef _DEBUG
  SB_DCHECK(current_thread_acquired_ == SbThreadGetCurrent());
#endif  // _DEBUG
}

#ifdef _DEBUG
void Mutex::debugInit() {
  current_thread_acquired_ = kSbThreadInvalid;
}
void Mutex::debugSetReleased() const {
  SbThread current_thread = SbThreadGetCurrent();
  SB_DCHECK(current_thread_acquired_ == current_thread);
  current_thread_acquired_ = kSbThreadInvalid;
}
void Mutex::debugPreAcquire() const {
  // Check that the mutex is not held by the current thread.
  SbThread current_thread = SbThreadGetCurrent();
  SB_DCHECK(current_thread_acquired_ != current_thread);
}
void Mutex::debugSetAcquired() const {
  // Check that the thread has already not been held.
  SB_DCHECK(current_thread_acquired_ == kSbThreadInvalid);
  current_thread_acquired_ = SbThreadGetCurrent();
}
#else
void Mutex::debugInit() {}
void Mutex::debugSetReleased() const {}
void Mutex::debugPreAcquire() const {}
void Mutex::debugSetAcquired() const {}
#endif

SbMutex* Mutex::mutex() const {
  return &mutex_;
}

ScopedLock::ScopedLock(const Mutex& mutex) : mutex_(mutex) {
  mutex_.Acquire();
}

ScopedLock::~ScopedLock() {
  mutex_.Release();
}

ScopedTryLock::ScopedTryLock(const Mutex& mutex) : mutex_(mutex) {
  is_locked_ = mutex_.AcquireTry();
}

ScopedTryLock::~ScopedTryLock() {
  if (is_locked_) {
    mutex_.Release();
  }
}

bool ScopedTryLock::is_locked() const {
  return is_locked_;
}

}  // namespace starboard
