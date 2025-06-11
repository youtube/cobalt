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
#include "starboard/android/shared/exoplayer/exoplayer.h"
#include "starboard/shared/starboard/player/player_internal.h"

void SbPlayerDestroy(SbPlayer player) {
  if (!SbPlayerIsValid(player)) {
    return;
  }

  // TODO: Remove //media/base:base dependency cycle to use base::FeatureList
  // here.
  if (/* base::FeatureList::IsEnabled(media::kCobaltUseExoPlayer) */ (true)) {
    SB_LOG(INFO) << "Using ExoPlayer SbPlayerDestroy() implementation;.";
    delete starboard::android::shared::exoplayer::ExoPlayer::
        GetExoPlayerForSbPlayer(player);
    return;
  }

  delete player;
}
