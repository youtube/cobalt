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

#include "starboard/android/shared/player_get_render_status.h"

#include "starboard/android/shared/render_status_func.h"
#include "starboard/common/log.h"
#include "starboard/extension/player_get_render_status.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/player_internal.h"

namespace starboard::android::shared {

namespace {

// Definitions of any functions included as components in the extension
// are added here.

void SetRenderStatusCBForCurrentThread(
    SbPlayerRenderStatusFunc render_status_func) {
  starboard::android::shared::SetRenderStatusCBForCurrentThread(
      render_status_func);
}

const StarboardExtensionPlayerGetRenderStatusApi kPlayerGetRenderStatusApi = {
    kStarboardExtensionPlayerGetRenderStatusName,
    1,
    &SetRenderStatusCBForCurrentThread,
};

}  // namespace

const void* GetPlayerGetRenderStatusApi() {
  return &kPlayerGetRenderStatusApi;
}

}  // namespace starboard::android::shared
