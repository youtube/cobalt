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

// clang-format off
#include "starboard/player.h"
// clang-format on

#include <jni.h>

#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/player/player_internal.h"
#include "third_party/jni_zero/jni_zero.h"

bool SbPlayerSetPlaybackRate(SbPlayer player, double playback_rate) {
  if (!SbPlayerIsValid(player)) {
    SB_DLOG(WARNING) << "player is invalid.";
    return false;
  }
  if (playback_rate < 0.0) {
    SB_DLOG(WARNING) << "playback_rate cannot be negative but it is set to "
                     << playback_rate << '.';
    return false;
  }

  player->SetPlaybackRate(playback_rate);

  // Keep the screen on while any player is playing. Note that a newly created
  // player isn't reported as playing, even though its playback rate defaults to
  // 1.0, as it cannot present anything before it is seeked, and the playback
  // rate is always set explicitly before playback starts (SbPlayerBridge sets
  // it to 0 on kSbPlayerStateInitialized, and to the pipeline's playback rate
  // after the seek).
  JNIEnv* env = jni_zero::AttachCurrentThread();
  if (playback_rate > 0.0) {
    starboard::StarboardBridge::GetInstance()->AddActivePlayer(
        env, reinterpret_cast<jlong>(player));
  } else {
    starboard::StarboardBridge::GetInstance()->RemoveActivePlayer(
        env, reinterpret_cast<jlong>(player));
  }
  return true;
}
