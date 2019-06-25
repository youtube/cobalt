// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/spin_lock.h"
#include "starboard/thread.h"

namespace starboard {

void SpinLockAcquire(SbAtomic32* atomic) {
  while (SbAtomicAcquire_CompareAndSwap(atomic, kSpinLockStateReleased,
                                        kSpinLockStateAcquired) ==
         kSpinLockStateAcquired) {
    SbThreadYield();
  }
}

void SpinLockRelease(SbAtomic32* atomic) {
  SbAtomicRelease_Store(atomic, kSpinLockStateReleased);
}

SpinLock::SpinLock() : atomic_(kSpinLockStateReleased) {}

SpinLock::~SpinLock() {}

void SpinLock::Acquire() {
  SpinLockAcquire(&atomic_);
}

void SpinLock::Release() {
  SpinLockRelease(&atomic_);
}

ScopedSpinLock::ScopedSpinLock(SbAtomic32* atomic) : atomic_(atomic) {
  SpinLockAcquire(atomic_);
}

ScopedSpinLock::ScopedSpinLock(SpinLock& spin_lock)
    : atomic_(&spin_lock.atomic_) {
  SpinLockAcquire(atomic_);
}

ScopedSpinLock::~ScopedSpinLock() {
  SpinLockRelease(atomic_);
}

}  // namespace starboard
