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

// Module Overview: Starboard Spin Lock module
//
// Defines a C++-only spin-lock implementation, built entirely on top of
// Starboard atomics and threads. It can be safely used by both clients and
// implementations.

#ifndef STARBOARD_COMMON_SPIN_LOCK_H_
#define STARBOARD_COMMON_SPIN_LOCK_H_

#include "starboard/atomic.h"

namespace starboard {

const SbAtomic32 kSpinLockStateReleased = 0;
const SbAtomic32 kSpinLockStateAcquired = 1;

void SpinLockAcquire(SbAtomic32* atomic);
void SpinLockRelease(SbAtomic32* atomic);

class SpinLock {
 public:
  SpinLock();
  ~SpinLock();
  void Acquire();
  void Release();

 private:
  SbAtomic32 atomic_;
  friend class ScopedSpinLock;
};

class ScopedSpinLock {
 public:
  explicit ScopedSpinLock(SbAtomic32* atomic);
  explicit ScopedSpinLock(SpinLock& spin_lock);
  ~ScopedSpinLock();

 private:
  SbAtomic32* atomic_;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_SPIN_LOCK_H_
