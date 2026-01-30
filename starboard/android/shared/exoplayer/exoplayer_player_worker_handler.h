// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_PLAYER_WORKER_HANDLER_H_
#define STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_PLAYER_WORKER_HANDLER_H_

#include <memory>
#include <string>

#include "starboard/android/shared/exoplayer/drm_system_exoplayer.h"
#include "starboard/android/shared/exoplayer/exoplayer_bridge.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/player_worker.h"

namespace starboard {

class ExoPlayerPlayerWorkerHandler : public PlayerWorker::Handler,
                                     private JobQueue::JobOwner {
 public:
  explicit ExoPlayerPlayerWorkerHandler(
      const SbPlayerCreationParam* creation_param);

 private:
  Result<void> Init(SbPlayer player,
                    UpdateMediaInfoCB update_media_info_cb,
                    GetPlayerStateCB get_player_state_cb,
                    UpdatePlayerStateCB update_player_state_cb,
                    UpdatePlayerErrorCB update_player_error_cb) override;
  Result<void> Seek(int64_t seek_to_time, int ticket) override;
  Result<void> WriteSamples(const InputBuffers& input_buffers,
                            int* samples_written) override;
  Result<void> WriteEndOfStream(SbMediaType sample_type) override;
  Result<void> SetPause(bool pause) override;
  Result<void> SetPlaybackRate(double playback_rate) override;
  void SetVolume(double volume) override;
  Result<void> SetBounds(const Bounds& bounds) override { return Success(); }

  void SetMaxVideoInputSize(int max_video_input_size) override {}
  void SetVideoSurfaceView(void* surface_view) override {}
  void Stop() override;

  SbDecodeTarget GetCurrentDecodeTarget() override {
    return kSbDecodeTargetInvalid;
  }

  void Update();
  void OnError(SbPlayerError error, const std::string& error_message);
  void OnPrerolled();
  void OnEnded();

  bool IsEOSWritten(SbMediaType type) const;

  void RunOnWorker(std::function<void()> task);

  UpdateMediaInfoCB update_media_info_cb_;
  GetPlayerStateCB get_player_state_cb_;
  UpdatePlayerStateCB update_player_state_cb_;
  UpdatePlayerErrorCB update_player_error_cb_;

  JobQueue::JobToken update_job_token_;
  const std::function<void()> update_job_;

  const std::unique_ptr<ExoPlayerBridge> bridge_;

  bool audio_eos_written_ = false;
  bool video_eos_written_ = false;

  const DrmSystemExoPlayer* drm_system_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_PLAYER_WORKER_HANDLER_H_
