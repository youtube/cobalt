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

#ifndef STARBOARD_COMMON_MEMORY_PRESSURE_REGISTRY_H_
#define STARBOARD_COMMON_MEMORY_PRESSURE_REGISTRY_H_

#include <mutex>
#include <vector>

#include "starboard/extension/memory_pressure.h"

namespace starboard {

class MemoryPressureObserver {
 public:
  virtual ~MemoryPressureObserver() = default;
  virtual void OnMemoryPressure(SbMemoryPressureLevel level) = 0;
};

class MemoryPressureRegistry {
 public:
  static MemoryPressureRegistry* GetInstance();

  void RegisterObserver(MemoryPressureObserver* observer);
  void UnregisterObserver(MemoryPressureObserver* observer);

  void NotifyMemoryPressure(SbMemoryPressureLevel level);

 private:
  MemoryPressureRegistry() = default;
  ~MemoryPressureRegistry() = default;

  MemoryPressureRegistry(const MemoryPressureRegistry&) = delete;
  MemoryPressureRegistry& operator=(const MemoryPressureRegistry&) = delete;

  std::mutex mutex_;
  std::vector<MemoryPressureObserver*> observers_;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_MEMORY_PRESSURE_REGISTRY_H_
