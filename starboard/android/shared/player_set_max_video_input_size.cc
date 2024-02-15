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

#include "starboard/android/shared/player_set_max_video_input_size.h"

#include "starboard/android/shared/video_max_video_input_size.h"
#include "starboard/extension/player_set_max_video_input_size.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

// Definitions of any functions included as components in the extension
// are added here.

void SetMaxVideoInputSizeForCurrentThread(int max_video_input_size) {
  starboard::android::shared::SetMaxVideoInputSizeForCurrentThread(
      max_video_input_size);
}

const StarboardExtensionPlayerSetMaxVideoInputSizeApi
    kPlayerSetMaxVideoInputSizeApi = {
        kStarboardExtensionPlayerSetMaxVideoInputSizeName,
        1,
        &SetMaxVideoInputSizeForCurrentThread,
};

}  // namespace

const void* GetPlayerSetMaxVideoInputSizeApi() {
  return &kPlayerSetMaxVideoInputSizeApi;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
