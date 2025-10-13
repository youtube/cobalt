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

#ifndef STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_WORKER_HANDLER_H_
#define STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_WORKER_HANDLER_H_

#include "starboard/shared/starboard/player/player_worker.h"

#include <memory>

#include "starboard/android/shared/exoplayer/exoplayer_bridge.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/player_worker.h"

namespace starboard {

class ExoPlayerWorkerHandler : public starboard::PlayerWorker::Handler,
                               private starboard::JobQueue::JobOwner {
 public:
  explicit ExoPlayerWorkerHandler(const SbPlayerCreationParam* creation_param);

 private:
  starboard::PlayerWorker::Handler::HandlerResult Init(
      SbPlayer player,
      UpdateMediaInfoCB update_media_info_cb,
      GetPlayerStateCB get_player_state_cb,
      UpdatePlayerStateCB update_player_state_cb,
      UpdatePlayerErrorCB update_player_error_cb) override;
  starboard::PlayerWorker::Handler::HandlerResult Seek(int64_t seek_to_time,
                                                       int ticket) override;
  starboard::PlayerWorker::Handler::HandlerResult WriteSamples(
      const InputBuffers& input_buffers,
      int* samples_written) override;
  starboard::PlayerWorker::Handler::HandlerResult WriteEndOfStream(
      SbMediaType sample_type) override;
  starboard::PlayerWorker::Handler::HandlerResult SetPause(bool pause) override;
  starboard::PlayerWorker::Handler::HandlerResult SetPlaybackRate(
      double playback_rate) override;
  void SetVolume(double volume) override;
  starboard::PlayerWorker::Handler::HandlerResult SetBounds(
      const Bounds& bounds) override {
    return {true};
  }

  void SetMaxVideoInputSize(int max_video_input_size) override {}
  void Stop() override;

  void Update();
  void OnError(SbPlayerError error, const std::string& error_message);
  void OnPrerolled();
  void OnEnded();

  SbDecodeTarget GetCurrentDecodeTarget() override {
    return kSbDecodeTargetInvalid;
  }

  std::unique_ptr<ExoPlayerBridge> bridge_;

  SbPlayer player_ = kSbPlayerInvalid;
  UpdateMediaInfoCB update_media_info_cb_;
  GetPlayerStateCB get_player_state_cb_;
  UpdatePlayerStateCB update_player_state_cb_;
  UpdatePlayerErrorCB update_player_error_cb_;

  bool paused_ = false;
  double playback_rate_ = 1.0;
  double volume_ = 1.0;
  JobQueue::JobToken update_job_token_;
  std::function<void()> update_job_;

  const starboard::AudioStreamInfo audio_stream_info_;
  const starboard::VideoStreamInfo video_stream_info_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_WORKER_HANDLER_H_
