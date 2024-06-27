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

#ifndef STARBOARD_EXTENSION_MEDIA_SETTINGS_H_
#define STARBOARD_EXTENSION_MEDIA_SETTINGS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionMediaSettingsName \
  "dev.cobalt.extension.MediaSettings"

typedef struct StarboardExtensionMediaSettingsApi {
  // Name should be the string |kStarboardExtensionMediaSettingsName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // This API enables the feature to async release MediaCodecBridge on Android
  // TV.
  void (*EnableAsyncReleaseMediaCodecBridge)(bool value);

  // This API set the timeout seconds for async release MediaCodecBridge on
  // Android TV.
  void (*SetAsyncReleaseMediaCodecBridgeTimeoutSeconds)(int timeout);

} StarboardExtensionMediaSettingsApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_MEDIA_SETTINGS_H_
