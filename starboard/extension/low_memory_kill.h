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

#ifndef STARBOARD_EXTENSION_LOW_MEMORY_KILL_H_
#define STARBOARD_EXTENSION_LOW_MEMORY_KILL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionLowMemoryKillName \
  "dev.starboard.extension.LowMemoryKill"

typedef struct StarboardExtensionLowMemoryKillApi {
  // Name should be the string |kStarboardExtensionLowMemoryKillName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Returns true if the previous Cobalt session was killed by the system due to
  // memory pressure.
  bool (*WasLowMemoryKilled)();

} StarboardExtensionLowMemoryKillApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_LOW_MEMORY_KILL_H_
