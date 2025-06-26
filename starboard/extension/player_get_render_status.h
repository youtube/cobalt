// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_EXTENSION_PLAYER_GET_RENDER_STATUS_H_
#define STARBOARD_EXTENSION_PLAYER_GET_RENDER_STATUS_H_

#include <stdint.h>

#include "starboard/common/player.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionPlayerGetRenderStatusName \
  "dev.starboard.extension.PlayerGetRenderStatus"

typedef struct StarboardExtensionPlayerGetRenderStatusApi {
  // Name should be the string
  // |kStarboardExtensionPlayerGetRenderStatusName|. This helps to
  // validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Returns the number of frames to be rendered. This can be used
  // to determine if audio or video is underflow.
  void (*SetRenderStatusCBForCurrentThread)(
      SbPlayerRenderStatusFunc render_status_func);
} StarboardExtensionPlayerGetRenderStatusApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_PLAYER_GET_RENDER_STATUS_H_
