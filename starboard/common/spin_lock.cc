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

#include <sched.h>

namespace starboard {

void SpinLockAcquire(std::atomic<int32_t>* atomic) {
  int expected{kSpinLockStateReleased};
  while (!atomic->compare_exchange_weak(expected, kSpinLockStateAcquired,
                                        std::memory_order_acquire)) {
    expected = kSpinLockStateReleased;
    sched_yield();
  }
}

void SpinLockRelease(std::atomic<int32_t>* atomic) {
  atomic->store(kSpinLockStateReleased, std::memory_order_release);
}

SpinLock::SpinLock() : atomic_(kSpinLockStateReleased) {}

SpinLock::~SpinLock() {}

void SpinLock::Acquire() {
  SpinLockAcquire(&atomic_);
}

void SpinLock::Release() {
  SpinLockRelease(&atomic_);
}

ScopedSpinLock::ScopedSpinLock(std::atomic<int32_t>* atomic) : atomic_(atomic) {
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
