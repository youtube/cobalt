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

#include "absl/synchronization/mutex.h"

#include "starboard/mutex.h"

#if !defined(STARBOARD)
#error This file is a specialization for Starboard only.
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN

// Note: This minimalistic implementation always uses the slow path so this is
// much less performant than the original Mutex. Letting the original Mutex
// implementation use PthreadWaiter or a StarboardWaiter should be much more
// performant.

void Mutex::AssertHeld() const {}

void Mutex::Lock() {
  intptr_t v = mu_.load(std::memory_order_relaxed);
  if (v) {
    SbMutexAcquire(reinterpret_cast<SbMutex*>(v));
  } else {
    // Need to create a new mutex, store in the |mu_| member variable.
    SbMutex* mutex = new SbMutex;
    intptr_t expected = 0;
    if (mu_.compare_exchange_strong(expected, reinterpret_cast<intptr_t>(mutex),
                                    std::memory_order_acquire,
                                    std::memory_order_relaxed)) {
      // We stored our mutex in the member variable.
      SbMutexAcquire(mutex);
    } else {
      // We raced with another thread creating a new mutex.
      delete mutex;
      SbMutexAcquire(
          reinterpret_cast<SbMutex*>(mu_.load(std::memory_order_relaxed)));
    }
  }
}
void Mutex::Unlock() {
  intptr_t v = mu_.load(std::memory_order_relaxed);
  if (v) {
    SbMutexRelease(reinterpret_cast<SbMutex*>(v));
  }
}

// Note: Since Starboard does not implement reader locks, these are mapped to
// |Lock| and |Unlock|. this version will not allow parallel readers and will
// deadlock when used re-entrant.

void Mutex::AssertReaderHeld() const {}

void Mutex::ReaderLock() {
  Lock();
}

void Mutex::ReaderUnlock() {
  Unlock();
}

Mutex::~Mutex() {
  intptr_t v = mu_.load(std::memory_order_relaxed);
  if (v) {
    SbMutex* mutex = reinterpret_cast<SbMutex*>(v);
    SbMutexDestroy(mutex);
    delete mutex;
  }
}

void Mutex::ForgetDeadlockInfo() {}

ABSL_NAMESPACE_END
}  // namespace absl
