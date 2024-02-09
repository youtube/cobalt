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

#ifndef STARBOARD_EXTENSION_PLAYER_SET_MAX_VIDEO_INPUT_SIZE_H_
#define STARBOARD_EXTENSION_PLAYER_SET_MAX_VIDEO_INPUT_SIZE_H_

#include <stdint.h>

#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/player.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionPlayerSetMaxVideoInputSizeName \
  "dev.starboard.extension.PlayerSetMaxVideoInputSize"

typedef struct StarboardExtensionPlayerSetMaxVideoInputSizeApi {
  // Name should be the string
  // |kStarboardExtensionPlayerSetMaxVideoInputSizeName|. This helps to validate
  // that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // 1. This is utilized to establish the maximum video input size
  //    for any subsequently created SbPlayer on the current calling thread.
  // 2. The maximum video input dimensions serve as a suggestion; the
  //    implementation is not obligated to adhere to it, and there is no
  //    feedback provided.
  // 3. Set it to 0 disable the setting.
  void (*SetMaxVideoInputSizeForCurrentThread)(int max_video_input_size);
} StarboardExtensionPlayerSetMaxVideoInputSizeApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_PLAYER_SET_MAX_VIDEO_INPUT_SIZE_H_
