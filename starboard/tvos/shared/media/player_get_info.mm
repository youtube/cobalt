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

#import <Foundation/Foundation.h>

#include <cstdint>
#include <limits>

#include "starboard/configuration.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/player_internal.h"
#import "starboard/tvos/shared/media/application_player.h"
#import "starboard/tvos/shared/media/player_manager.h"
#import "starboard/tvos/shared/starboard_application.h"

namespace {

// Converts a POSIX microseconds timestamp to a Windows microseconds timestamp.
SB_C_FORCE_INLINE int64_t PosixTimeToWindowsTime(int64_t posix_time) {
  // Add number of microseconds since Jan 1, 1601 (UTC) until Jan 1, 1970 (UTC).
  return posix_time + 11644473600000000ULL;
}

}  // namespace

void SbPlayerGetInfo(SbPlayer player, SbPlayerInfo* out_player_info) {
  if (!player) {
    return;
  }
  if (!out_player_info) {
    return;
  }
  @autoreleasepool {
    SBDPlayerManager* playerManager = SBDGetApplication().playerManager;
    if ([playerManager isUrlPlayer:player]) {
      // Url player process
      SBDApplicationPlayer* applicationPlayer =
          [playerManager applicationPlayerForStarboardPlayer:player];

      out_player_info->current_media_timestamp =
          applicationPlayer.currentMediaTime;

      if (applicationPlayer.duration == NSIntegerMax) {
        out_player_info->duration = std::numeric_limits<int64_t>::max();
      } else {
        out_player_info->duration = applicationPlayer.duration;
      }

      out_player_info->frame_width =
          static_cast<int>(applicationPlayer.frameWidth);
      out_player_info->frame_height =
          static_cast<int>(applicationPlayer.frameHeight);
      out_player_info->is_paused = applicationPlayer.paused;
      out_player_info->playback_rate = applicationPlayer.playbackRate;
      out_player_info->volume = applicationPlayer.volume;
      out_player_info->dropped_video_frames =
          applicationPlayer.totalDroppedFrames;
      out_player_info->total_video_frames = applicationPlayer.totalFrames;

      NSTimeInterval currentDate = applicationPlayer.currentDate;
      if (currentDate > 0) {
        int64_t posixTime = currentDate * 1000000;
        int64_t startDate = PosixTimeToWindowsTime(posixTime);
        int64_t currentMediaTime = out_player_info->current_media_timestamp;
        if (currentMediaTime > 0) {
          startDate -= currentMediaTime;
        }
        out_player_info->start_date = startDate;
      } else {
        out_player_info->start_date = 0;
      }

      // Unfilled fields
      out_player_info->corrupted_video_frames = 0;
      return;
    }
  }
  // Normal player process
  player->GetInfo(out_player_info);
}
