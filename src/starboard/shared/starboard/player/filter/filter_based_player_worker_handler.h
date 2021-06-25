// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <functional>
#include <string>

#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
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

class FilterBasedPlayerWorkerHandler : public PlayerWorker::Handler,
                                       private JobQueue::JobOwner {
 public:
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  FilterBasedPlayerWorkerHandler(
      const SbPlayerCreationParam* creation_param,
      SbDecodeTargetGraphicsContextProvider* provider);
#else   // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  FilterBasedPlayerWorkerHandler(
      SbMediaVideoCodec video_codec,
      SbMediaAudioCodec audio_codec,
      SbDrmSystem drm_system,
      const SbMediaAudioSampleInfo* audio_sample_info,
      SbPlayerOutputMode output_mode,
      SbDecodeTargetGraphicsContextProvider* provider);
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

 private:
  bool Init(SbPlayer player,
            UpdateMediaInfoCB update_media_info_cb,
            GetPlayerStateCB get_player_state_cb,
            UpdatePlayerStateCB update_player_state_cb,
            UpdatePlayerErrorCB update_player_error_cb,
            std::string* error_message) override;
  bool Seek(SbTime seek_to_time, int ticket) override;
  bool WriteSample(const scoped_refptr<InputBuffer>& input_buffer,
                   bool* written) override;
  bool WriteEndOfStream(SbMediaType sample_type) override;
  bool SetPause(bool pause) override;
  bool SetPlaybackRate(double playback_rate) override;
  void SetVolume(double volume) override;
  bool SetBounds(const Bounds& bounds) override;
  void Stop() override;

  void Update();
  void OnError(SbPlayerError error, const std::string& error_message);
  void OnPrerolled(SbMediaType media_type);
  void OnEnded(SbMediaType media_type);

  SbDecodeTarget GetCurrentDecodeTarget() override;

  SbPlayer player_ = kSbPlayerInvalid;
  UpdateMediaInfoCB update_media_info_cb_;
  GetPlayerStateCB get_player_state_cb_;
  UpdatePlayerStateCB update_player_state_cb_;
  UpdatePlayerErrorCB update_player_error_cb_;

  SbMediaVideoCodec video_codec_;
  SbMediaAudioCodec audio_codec_;
  SbDrmSystem drm_system_;

  media::AudioSampleInfo audio_sample_info_;

  // A mutex guarding changes to the existence (e.g. creation/destruction)
  // of the |player_components_| object.  This is necessary because calls to
  // GetCurrentDecodeTarget() need to be supported on arbitrary threads, and
  // GetCurrentDecodeTarget() will call into the video renderer held by
  // |player_components_|.  This mutex only needs to be held when any thread
  // creates or resets |player_components_|, and when GetCurrentDecodeTarget()
  // reads from the video renderer held by |player_components_|.  This is
  // because GetCurrentDecodeTarget() won't modify |player_components_|, and all
  // other accesses are happening from the same thread.
  Mutex player_components_existence_mutex_;

  scoped_ptr<PlayerComponents> player_components_;
  // The following three variables cache the return values of member functions
  // of |player_components_|.  Their lifetime is tied to |player_components_|.
  MediaTimeProvider* media_time_provider_ = nullptr;
  AudioRenderer* audio_renderer_ = nullptr;
  VideoRenderer* video_renderer_ = nullptr;

  bool paused_ = false;
  double playback_rate_ = 1.0;
  double volume_ = 1.0;
  PlayerWorker::Bounds bounds_ = {};
  JobQueue::JobToken update_job_token_;
  std::function<void()> update_job_;

  bool audio_prerolled_ = false;
  bool video_prerolled_ = false;
  bool audio_ended_ = false;
  bool video_ended_ = false;

  SbPlayerOutputMode output_mode_;
  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider_;

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  media::VideoSampleInfo video_sample_info_;
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_FILTER_BASED_PLAYER_WORKER_HANDLER_H_
