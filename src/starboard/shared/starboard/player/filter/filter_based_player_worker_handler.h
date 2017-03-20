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
#if SB_API_VERSION >= 3
#include "starboard/decode_target.h"
#endif
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/closure.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
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
                                 const SbMediaAudioHeader& audio_header
#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
                                 ,
                                 SbPlayerOutputMode output_mode,
                                 SbDecodeTargetProvider* provider
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
                                 );  // NOLINT(whitespace/parens)

 private:
  bool Init(PlayerWorker* player_worker,
            JobQueue* job_queue,
            SbPlayer player,
            UpdateMediaTimeCB update_media_time_cb,
            GetPlayerStateCB get_player_state_cb,
            UpdatePlayerStateCB update_player_state_cb) SB_OVERRIDE;
  bool Seek(SbMediaTime seek_to_pts, int ticket) SB_OVERRIDE;
  bool WriteSample(InputBuffer input_buffer, bool* written) SB_OVERRIDE;
  bool WriteEndOfStream(SbMediaType sample_type) SB_OVERRIDE;
  bool SetPause(bool pause) SB_OVERRIDE;
#if SB_API_VERSION >= SB_PLAYER_SET_PLAYBACK_RATE_VERSION
  bool SetPlaybackRate(double playback_rate) SB_OVERRIDE;
#endif  // SB_API_VERSION >= SB_PLAYER_SET_PLAYBACK_RATE_VERSION
  bool SetBounds(const PlayerWorker::Bounds& bounds) SB_OVERRIDE;
  void Stop() SB_OVERRIDE;

  void Update();

#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
  SbDecodeTarget GetCurrentDecodeTarget() SB_OVERRIDE;
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

  PlayerWorker* player_worker_;
  JobQueue* job_queue_;
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
  PlayerWorker::Bounds bounds_;
  Closure update_closure_;

  // A mutex guarding changes to the existence (e.g. creation/destruction)
  // of the |video_renderer_| object.  This is necessary because calls to
  // GetCurrentDecodeTarget() need to be supported on arbitrary threads, and
  // GetCurrentDecodeTarget() will call into |video_renderer_|.  This mutex
  // only needs to be held when any thread creates or resets |video_renderer_|,
  // and when GetCurrentDecodeTarget() reads from it.  This is because
  // GetCurrentDecodeTarget() won't modify |video_renderer_|, and all other
  // accesses are happening from the same thread.
  ::starboard::Mutex video_renderer_existence_mutex_;

#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
  SbPlayerOutputMode output_mode_;
  SbDecodeTargetProvider* decode_target_provider_;
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_FILTER_BASED_PLAYER_WORKER_HANDLER_H_
