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

namespace starboard {

Mutex::Mutex() : mutex_() {
  pthread_mutex_init(&mutex_, nullptr);
}

Mutex::~Mutex() {
  pthread_mutex_destroy(&mutex_);
}

void Mutex::Acquire() const {
  mutex_debugger_.debugPreAcquire();
  pthread_mutex_lock(&mutex_);
  mutex_debugger_.debugSetAcquired();
}

bool Mutex::AcquireTry() const {
  bool ok = pthread_mutex_trylock(&mutex_) == 0;
  if (ok) {
    mutex_debugger_.debugSetAcquired();
  }
  return ok;
}

void Mutex::Release() const {
  mutex_debugger_.debugSetReleased();
  pthread_mutex_unlock(&mutex_);
}

void Mutex::DCheckAcquired() const {
#ifdef _DEBUG
  SB_DCHECK(
      pthread_equal(mutex_debugger_.current_thread_acquired_, pthread_self()));
#endif  // _DEBUG
}

pthread_mutex_t* Mutex::mutex() const {
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
