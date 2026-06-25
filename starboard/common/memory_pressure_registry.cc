// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/memory_pressure_registry.h"

#include <algorithm>

#include "starboard/common/log.h"

namespace starboard {

// static
MemoryPressureRegistry* MemoryPressureRegistry::GetInstance() {
  static MemoryPressureRegistry* instance = new MemoryPressureRegistry();
  return instance;
}

void MemoryPressureRegistry::RegisterObserver(
    MemoryPressureObserver* observer) {
  std::lock_guard<std::mutex> lock(mutex_);
  SB_DCHECK(std::find(observers_.begin(), observers_.end(), observer) == observers_.end());
  if (std::find(observers_.begin(), observers_.end(), observer) == observers_.end()) {
    observers_.push_back(observer);
  }
}

void MemoryPressureRegistry::UnregisterObserver(
    MemoryPressureObserver* observer) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = std::find(observers_.begin(), observers_.end(), observer);
  if (it != observers_.end()) {
    observers_.erase(it);
  }

  // Block until any active notification callback running on another thread
  // for this observer completes. This guarantees that the observer object
  // is no longer being accessed by the registry and can be safely destroyed.
  cv_.wait(lock, [this, observer] {
    return std::find(active_notifications_.begin(),
                     active_notifications_.end(),
                     observer) == active_notifications_.end();
  });
}

void MemoryPressureRegistry::NotifyMemoryPressure(SbMemoryPressureLevel level) {
  std::vector<MemoryPressureObserver*> observers_copy;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_copy = observers_;
  }

  for (MemoryPressureObserver* observer : observers_copy) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      // Check if the observer was unregistered while we were looping.
      if (std::find(observers_.begin(), observers_.end(), observer) == observers_.end()) {
        continue;
      }
      active_notifications_.push_back(observer);
    }

    // Invoke the callback outside the registry lock to prevent lock inversion/deadlocks.
    observer->OnMemoryPressure(level);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = std::find(active_notifications_.begin(), active_notifications_.end(), observer);
      if (it != active_notifications_.end()) {
        active_notifications_.erase(it);
      }
    }
    cv_.notify_all();
  }
}

}  // namespace starboard
