// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/tvos/shared/observer_registry.h"

#include "starboard/common/log.h"
#include "starboard/common/spin_lock.h"

namespace starboard {

namespace {
struct ObserverEntry {
  const ObserverRegistry::Observer* volatile observer = nullptr;
  volatile int32_t observer_id;
  std::atomic_int32_t lock{kSpinLockStateReleased};
};

// Use global data in case registered observers might be called after shutdown.
std::atomic_int32_t g_observers_lock{kSpinLockStateReleased};
constexpr int32_t kMaxObservers = 384;
ObserverEntry g_observers[kMaxObservers];
}  // namespace

volatile std::atomic_int32_t ObserverRegistry::Observer::id_counter{0};

// static
void ObserverRegistry::RegisterObserver(const Observer* observer) {
  SB_DCHECK(observer != nullptr);
  ScopedSpinLock lock(&g_observers_lock);

  // Claim an observer slot.
  for (int32_t i = 0;; ++i) {
    // If there are no more slots, then kMaxObservers should be increased or
    // observers are being leaked.
    SB_CHECK(i < kMaxObservers);
    if (g_observers[i].observer == nullptr) {
      // Lock the slot to avoid race conditions with LockObserver().
      ScopedSpinLock observer_lock(&g_observers[i].lock);
      g_observers[i].observer = observer;
      g_observers[i].observer_id = observer->id;
      break;
    }
  }
}

// static
void ObserverRegistry::UnregisterObserver(const Observer* observer) {
  SB_DCHECK(observer != nullptr);
  ScopedSpinLock lock(&g_observers_lock);

  // Release the observer slot.
  for (int32_t i = 0;; ++i) {
    SB_CHECK(i < kMaxObservers);
    if (g_observers[i].observer == observer) {
      // Lock the slot to avoid race conditions with LockObserver().
      ScopedSpinLock observer_lock(&g_observers[i].lock);
      g_observers[i].observer = nullptr;
      SB_DCHECK(g_observers[i].observer_id == observer->id);
      break;
    }
  }
}

// static
int32_t ObserverRegistry::LockObserver(const Observer* observer) {
  SB_DCHECK(observer != nullptr);
  // |observer| may have been deallocated. First find a slot to verify that
  // |observer| is valid, then verify the |observer->id| to double-check that
  // this isn't a late call for an old observer.
  for (int32_t i = 0; i < kMaxObservers; ++i) {
    if (g_observers[i].observer == observer) {
      // Lock the slot to avoid race conditions with register / unregister.
      SpinLockAcquire(&g_observers[i].lock);
      // Now that the slot is locked, double-check the observer and id.
      if (g_observers[i].observer == observer &&
          g_observers[i].observer_id == observer->id) {
        return i;
      } else {
        SpinLockRelease(&g_observers[i].lock);
      }
    }
  }

  // The |observer| was already unregistered.
  return -1;
}

// static
void ObserverRegistry::UnlockObserver(int32_t lock_slot) {
  SB_CHECK(lock_slot >= 0 && lock_slot < kMaxObservers);
  SB_DCHECK(g_observers[lock_slot].observer != nullptr);
  SB_DCHECK(g_observers[lock_slot].lock == kSpinLockStateAcquired);
  SpinLockRelease(&g_observers[lock_slot].lock);
}

}  // namespace starboard
