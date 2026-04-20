// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_EXTENSION_SYSTEM_INFO_H_
#define STARBOARD_EXTENSION_SYSTEM_INFO_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionSystemInfoName "dev.starboard.extension.SystemInfo"

typedef struct StarboardExtensionSystemInfoApi {
  // Name should be the string |kStarboardExtensionSystemInfoName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  int64_t (*GetAppStartWithAndroidFix)();
} StarboardExtensionSystemInfoApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_SYSTEM_INFO_H_
