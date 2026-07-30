// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_EXTENSION_FREE_SPACE_H_
#define STARBOARD_EXTENSION_FREE_SPACE_H_

#include <stdint.h>

#include "starboard/system.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionFreeSpaceName "dev.cobalt.extension.FreeSpace"

typedef struct CobaltExtensionFreeSpaceApi {
  // Name should be the string |kCobaltExtensionFreeSpaceName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Returns the free space in bytes for the provided |system_path_id|.
  // If there is no implementation for the that |system_path_id| or
  // if there was an error -1 is returned.
  int64_t (*MeasureFreeSpace)(SbSystemPathId system_path_id);
} CobaltExtensionFreeSpaceApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_FREE_SPACE_H_
