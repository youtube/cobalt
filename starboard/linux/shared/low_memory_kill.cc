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

#include "starboard/linux/shared/low_memory_kill.h"

#include "starboard/extension/low_memory_kill.h"

namespace starboard {

namespace {

bool WasLowMemoryKilled() {
  return false;
}

const StarboardExtensionLowMemoryKillApi kLowMemoryKillApi = {
    kStarboardExtensionLowMemoryKillName,
    1,  // API version that's implemented.
    &WasLowMemoryKilled,
};

}  // namespace

const void* GetLowMemoryKillApi() {
  return &kLowMemoryKillApi;
}

}  // namespace starboard
