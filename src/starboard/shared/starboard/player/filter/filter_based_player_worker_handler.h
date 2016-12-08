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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_FILTER_BASED_PLAYER_WORKER_HANDLER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_FILTER_BASED_PLAYER_WORKER_HANDLER_H_

#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/player_worker.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class FilterBasedPlayerWorkerHandler : public PlayerWorker::Handler {
 public:
  FilterBasedPlayerWorkerHandler(SbMediaVideoCodec video_codec,
                                 SbMediaAudioCodec audio_codec,
                                 SbDrmSystem drm_system,
                                 const SbMediaAudioHeader& audio_header);

 private:
  void Setup(PlayerWorker* player_worker,
             SbPlayer player,
             UpdateMediaTimeCB update_media_time_cb,
             GetPlayerStateCB get_player_state_cb,
             UpdatePlayerStateCB update_player_state_cb) SB_OVERRIDE;
  bool ProcessInitEvent() SB_OVERRIDE;
  bool ProcessSeekEvent(const SeekEventData& data) SB_OVERRIDE;
  bool ProcessWriteSampleEvent(const WriteSampleEventData& data,
                               bool* written) SB_OVERRIDE;
  bool ProcessWriteEndOfStreamEvent(const WriteEndOfStreamEventData& data)
      SB_OVERRIDE;
  bool ProcessSetPauseEvent(const SetPauseEventData& data) SB_OVERRIDE;
  bool ProcessUpdateEvent(const SetBoundsEventData& data) SB_OVERRIDE;
  void ProcessStopEvent() SB_OVERRIDE;

  PlayerWorker* player_worker_;
  SbPlayer player_;
  UpdateMediaTimeCB update_media_time_cb_;
  GetPlayerStateCB get_player_state_cb_;
  UpdatePlayerStateCB update_player_state_cb_;

  SbMediaVideoCodec video_codec_;
  SbMediaAudioCodec audio_codec_;
  SbDrmSystem drm_system_;
  SbMediaAudioHeader audio_header_;

  scoped_ptr<AudioRenderer> audio_renderer_;
  scoped_ptr<VideoRenderer> video_renderer_;

  bool paused_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_FILTER_BASED_PLAYER_WORKER_HANDLER_H_
