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

#include "starboard/android/shared/player_get_number_of_frames_to_be_rendered.h"

#include "starboard/common/log.h"
#include "starboard/extension/player_get_number_of_frames_to_be_rendered.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/player_internal.h"

namespace starboard::android::shared {

namespace {

// Definitions of any functions included as components in the extension
// are added here.

int32_t GetNumberOfFramesToBeRendered(uint64_t player) {
  starboard::shared::starboard::player::SbPlayerPrivateImpl* player_ptr =
      reinterpret_cast<
          starboard::shared::starboard::player::SbPlayerPrivateImpl*>(player);
  return player_ptr->GetNumberOfFramesToBeRendered();
}

const StarboardExtensionPlayerGetNumberOfFramesToBeRenderedApi
    kPlayerGetNumberOfFramesToBeRenderedApi = {
        kStarboardExtensionPlayerGetNumberOfFramesToBeRenderedName,
        1,
        &GetNumberOfFramesToBeRendered,
};

}  // namespace

const void* GetPlayerGetNumberOfFramesToBeRenderedApi() {
  return &kPlayerGetNumberOfFramesToBeRenderedApi;
}

}  // namespace starboard::android::shared
