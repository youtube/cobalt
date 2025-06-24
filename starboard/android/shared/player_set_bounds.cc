// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/player.h"

// TODO: Remove //media/base:base dependency cycle to use base::FeatureList
// here. #include "media/base/media_switches.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/player/player_internal.h"

// TODO: (cobalt b/372559388) Update namespace to jni_zero.

void SbPlayerSetBounds(SbPlayer player,
                       int z_index,
                       int x,
                       int y,
                       int width,
                       int height) {
  if (!SbPlayerIsValid(player)) {
    SB_DLOG(WARNING) << "player is invalid.";
    return;
  }
  SB_LOG(INFO) << "Starting to set bounds";
  starboard::android::shared::JniEnvExt::Get()->CallStarboardVoidMethodOrAbort(
      "setVideoSurfaceBounds", "(IIII)V", x, y, width, height);
  SB_LOG(INFO) << "Ending set bounds";
  // TODO: Remove //media/base:base dependency cycle to use base::FeatureList
  // here.
  if (/* !base::FeatureList::IsEnabled(media::kCobaltUseExoPlayer) */ (true)) {
    // Skip fowarding the bounds to the ExoPlayer.
    player->SetBounds(z_index, x, y, width, height);
  }
}
