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

#ifndef STARBOARD_EXTENSION_PLAYER_SETTINGS_H_
#define STARBOARD_EXTENSION_PLAYER_SETTINGS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionPlayerSettingsName \
  "dev.starboard.extension.PlayerSettings"

typedef struct StarboardExtensionPlayerSettingsApi {
  // Name should be the string kStarboardExtensionPlayerSettingsName.
  const char* name;

  // Version of the API (1).
  uint32_t version;

  // Sets the maximum video input size for any subsequently created
  // SbPlayer on the current calling thread. Set to 0 to disable.
  void (*SetMaxVideoInputSizeForCurrentThread)(int max_video_input_size);

  // Sets the SurfaceView (jobject) for any subsequently created
  // SbPlayer on the current calling thread. Set to NULL to clear.
  void (*SetVideoSurfaceViewForCurrentThread)(void* surface_view);
} StarboardExtensionPlayerSettingsApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_PLAYER_SETTINGS_H_
