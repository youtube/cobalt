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

#include <cstring>

#include "cobalt/browser/h5vcc_native_stability/low_memory_kill.h"
#include "starboard/extension/low_memory_kill.h"
#include "starboard/system.h"

namespace h5vcc_native_stability {

bool GetWasLowMemoryKilled() {
  const auto* low_memory_kill_extension =
      static_cast<const StarboardExtensionLowMemoryKillApi*>(
          SbSystemGetExtension(kStarboardExtensionLowMemoryKillName));
  if (!low_memory_kill_extension || !low_memory_kill_extension->name ||
      strcmp(low_memory_kill_extension->name,
             kStarboardExtensionLowMemoryKillName) != 0 ||
      low_memory_kill_extension->version < 1 ||
      !low_memory_kill_extension->WasLowMemoryKilled) {
    return false;
  }
  return low_memory_kill_extension->WasLowMemoryKilled();
}

}  // namespace h5vcc_native_stability
