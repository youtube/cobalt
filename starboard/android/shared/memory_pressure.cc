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

#include "starboard/android/shared/memory_pressure.h"

#include "starboard/common/memory_pressure_registry.h"
#include "starboard/extension/memory_pressure.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

const uint32_t kMemoryPressureApiVersion = 1u;

void NotifyMemoryPressure(SbMemoryPressureLevel level) {
  MemoryPressureRegistry::GetInstance()->NotifyMemoryPressure(level);
}

constexpr StarboardExtensionMemoryPressureApi kMemoryPressureApi = {
    kStarboardExtensionMemoryPressureName,
    kMemoryPressureApiVersion,
    &NotifyMemoryPressure,
};

}  // namespace

const void* GetMemoryPressureApi() {
  return &kMemoryPressureApi;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
