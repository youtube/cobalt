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

#ifndef STARBOARD_EXTENSION_PLATFORM_INFO_H_
#define STARBOARD_EXTENSION_PLATFORM_INFO_H_

#include <stdint.h>

#include "starboard/system.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionPlatformInfoName "dev.cobalt.extension.PlatformInfo"

typedef struct CobaltExtensionPlatformInfoApi {
  // Name should be the string |kCobaltExtensionPlatformInfoName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Returns details about the device firmware version. On Android, this will
  // be the Android build fingerprint (go/android-build-fingerprint).
  bool (*GetFirmwareVersionDetails)(char* out_value, int value_length);

  // Returns the OS experience. (e.g. Amati or Watson on an Android device).
  const char* (*GetOsExperience)();

  // Returns the Core Services version (e.g. the Google Play Services package
  // version on an Android device).
  int64_t (*GetCoreServicesVersion)();
} CobaltExtensionPlatformInfoApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_PLATFORM_INFO_H_
