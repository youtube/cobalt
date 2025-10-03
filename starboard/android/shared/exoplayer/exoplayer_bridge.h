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

#ifndef STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_BRIDGE_H_
#define STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_BRIDGE_H_

#include <memory>
#include <string>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard {
namespace android {
namespace shared {

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::VideoSurfaceHolder;
using starboard::shared::starboard::player::JobQueue;
using starboard::shared::starboard::player::JobThread;

class ExoPlayerBridge final : private VideoSurfaceHolder {
 public:
  typedef std::function<void(int64_t media_time,
                             int dropped_video_frames,
                             int ticket,
                             bool is_progressing)>
      UpdateMediaInfoCB;

  ExoPlayerBridge(const SbPlayerCreationParam* creation_param,
                  SbPlayerDeallocateSampleFunc sample_deallocate_func,
                  SbPlayerDecoderStatusFunc decoder_status_func,
                  SbPlayerStatusFunc player_status_func,
                  SbPlayerErrorFunc player_error_func,
                  UpdateMediaInfoCB update_media_info_cb,
                  SbPlayer player,
                  void* context);
  ~ExoPlayerBridge();

  void Seek(int64_t seek_to_timestamp, int ticket);
  void WriteSamples(SbMediaType sample_type,
                    const SbPlayerSampleInfo* sample_infos,
                    int number_of_sample_infos);
  void WriteEndOfStream(SbMediaType stream_type) const;
  bool Play();
  bool Pause();
  bool Stop();
  bool SetVolume(double volume);

  void OnPlayerInitialized();
  void OnPlayerPrerolled();
  void OnPlayerError();
  void SetPlayingStatus(bool is_playing);

  // VideoSurfaceHolder method
  void OnSurfaceDestroyed() override {}

  bool is_valid() const { return j_exoplayer_bridge_ != nullptr; }

 private:
  bool InitExoplayer();
  void DoSeek(int64_t seek_to_timestamp, int ticket);
  void DoWriteSamples(SbMediaType sample_type,
                      const SbPlayerSampleInfo* sample_infos,
                      int number_of_sample_infos);
  void DoWriteEndOfStream(SbMediaType stream_type) const;
  void DoPlay();
  void DoPause();
  void DoStop();
  void DoSetVolume(double volume);

  void Update();

  void TearDownExoPlayer();
  void UpdatePlayingStatus(bool is_playing);
  void UpdatePlayerState(SbPlayerState player_state);
  void UpdateDecoderState(SbMediaType type, SbPlayerDecoderState state);
  void UpdatePlayerError(SbPlayerError error, const std::string& error_message);

  SbPlayerDeallocateSampleFunc sample_deallocate_func_;
  SbPlayerDecoderStatusFunc decoder_status_func_;
  SbPlayerStatusFunc player_status_func_;
  SbPlayerErrorFunc player_error_func_;
  UpdateMediaInfoCB update_media_info_cb_;

  jobject j_exoplayer_bridge_ = nullptr;
  jobject j_sample_data_ = nullptr;
  jobject j_output_surface_ = nullptr;

  void* context_;
  SbPlayer player_;
  bool error_occurred_ = false;
  int ticket_;
  SbPlayerState player_state_;
  std::unique_ptr<JobThread> exoplayer_thread_;
  starboard::shared::starboard::media::AudioStreamInfo audio_stream_info_;

  int64_t last_media_time_;
  int dropped_video_frames_;
  bool is_progressing_;

  JobQueue::JobToken update_job_token_;
  std::function<void()> update_job_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_BRIDGE_H_
