// Copyright 2016 Google Inc. All Rights Reserved.
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

// A C++-only spin-lock implementation, built entirely on top of
// Starboard atomics and threads. Can be safely used by both clients and
// implementations.

#ifndef STARBOARD_SPIN_LOCK_H_
#define STARBOARD_SPIN_LOCK_H_

#include "starboard/atomic.h"
#include "starboard/thread.h"

#ifndef __cplusplus
#error "Only C++ files can include this header."
#endif

namespace starboard {

const SbAtomic32 kSpinLockStateReleased = 0;
const SbAtomic32 kSpinLockStateAcquired = 1;

inline void SpinLockAcquire(SbAtomic32* atomic) {
  while (SbAtomicAcquire_CompareAndSwap(atomic, kSpinLockStateReleased,
                                        kSpinLockStateAcquired) ==
         kSpinLockStateAcquired) {
    SbThreadYield();
  }
}

inline void SpinLockRelease(SbAtomic32* atomic) {
  SbAtomicRelease_Store(atomic, kSpinLockStateReleased);
}

class SpinLock {
 public:
  SpinLock() : atomic_(kSpinLockStateReleased) {}
  ~SpinLock() {}
  void Acquire() { SpinLockAcquire(&atomic_); }
  void Release() { SpinLockRelease(&atomic_); }

 private:
  SbAtomic32 atomic_;
  friend class ScopedSpinLock;
};

class ScopedSpinLock {
 public:
  explicit ScopedSpinLock(SbAtomic32* atomic) : atomic_(atomic) {
    SpinLockAcquire(atomic_);
  }
  explicit ScopedSpinLock(SpinLock& spin_lock) : atomic_(&spin_lock.atomic_) {
    SpinLockAcquire(atomic_);
  }
  ~ScopedSpinLock() { SpinLockRelease(atomic_); }

 private:
  SbAtomic32* atomic_;
};

}  // namespace starboard

#endif  // STARBOARD_SPIN_LOCK_H_
