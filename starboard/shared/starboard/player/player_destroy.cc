// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/shared/media_session/playback_state.h"
#include "starboard/shared/starboard/player/player_internal.h"
#if SB_PLAYER_ENABLE_VIDEO_DUMPER
#include "starboard/shared/starboard/player/video_dmp_writer.h"
#endif  // SB_PLAYER_ENABLE_VIDEO_DUMPER

using starboard::shared::media_session::kNone;
using starboard::shared::media_session::
    UpdateActiveSessionPlatformPlaybackState;

void SbPlayerDestroy(SbPlayer player) {
  if (!SbPlayerIsValid(player)) {
    return;
  }
  UpdateActiveSessionPlatformPlaybackState(kNone);

#if SB_PLAYER_ENABLE_VIDEO_DUMPER
  using ::starboard::shared::starboard::player::video_dmp::VideoDmpWriter;
  VideoDmpWriter::OnPlayerDestroy(player);
#endif  // SB_PLAYER_ENABLE_VIDEO_DUMPER

  delete player;
}
