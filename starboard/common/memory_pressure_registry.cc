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

namespace starboard {

// static
MemoryPressureRegistry* MemoryPressureRegistry::GetInstance() {
  static MemoryPressureRegistry* instance = new MemoryPressureRegistry();
  return instance;
}

void MemoryPressureRegistry::RegisterObserver(
    MemoryPressureObserver* observer) {
  std::lock_guard<std::mutex> lock(mutex_);
  observers_.push_back(observer);
}

void MemoryPressureRegistry::UnregisterObserver(
    MemoryPressureObserver* observer) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = std::find(observers_.begin(), observers_.end(), observer);
  if (it != observers_.end()) {
    observers_.erase(it);
  }
}

void MemoryPressureRegistry::NotifyMemoryPressure(SbMemoryPressureLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto* observer : observers_) {
    observer->OnMemoryPressure(level);
  }
}

}  // namespace starboard
