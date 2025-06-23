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

#include <pthread.h>

#include <memory>
#include <string>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/common/mutex.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard::android::shared::exoplayer {

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::VideoSurfaceHolder;
using starboard::shared::starboard::player::filter::EndedCB;
using starboard::shared::starboard::player::filter::ErrorCB;

class ExoPlayerBridge final : private VideoSurfaceHolder {
 public:
  typedef ::starboard::shared::starboard::player::filter::PrerolledCB
      PrerolledCB;
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;
  typedef ::starboard::shared::starboard::player::InputBuffers InputBuffers;
  typedef ::starboard::shared::starboard::media::AudioStreamInfo
      AudioStreamInfo;
  typedef ::starboard::shared::starboard::media::VideoStreamInfo
      VideoStreamInfo;

  struct MediaInfo {
    bool is_playing;
    bool is_eos_played;
    bool is_underflow;
    double playback_rate;
  };
  //   typedef std::function<void(MediaInfo& info)>
  //       UpdateMediaInfoCB;

  ExoPlayerBridge(const AudioStreamInfo& audio_stream_info,
                  const VideoStreamInfo& video_stream_info,
                  const ErrorCB& error_cb,
                  const PrerolledCB& prerolled_cb,
                  const EndedCB& ended_cb);
  ~ExoPlayerBridge();

  void Seek(int64_t seek_to_timestamp);
  void WriteSamples(const InputBuffers& input_buffers);
  void WriteEndOfStream(SbMediaType stream_type);
  bool Play();
  bool Pause();
  bool Stop();
  bool SetVolume(double volume);
  void SetPlaybackRate(const double playback_rate);

  int GetDroppedFrames();
  int64_t GetCurrentMediaTime(MediaInfo& info);

  void OnPlayerInitialized();
  void OnPlayerPrerolled();
  void OnPlayerError();
  void OnPlaybackEnded();
  void SetPlayingStatus(bool is_playing);

  // VideoSurfaceHolder method
  void OnSurfaceDestroyed() override {}

  bool IsEndOfStreamWritten(SbMediaType type) {
    return type == kSbMediaTypeAudio ? audio_eos_written_ : video_eos_written_;
  }

  bool is_valid() const { return j_exoplayer_bridge_ != nullptr; }

 private:
  bool InitExoplayer();

  void TearDownExoPlayer();
  void UpdatePlayingStatus(bool is_playing);

  jobject j_exoplayer_bridge_ = nullptr;
  jobject j_sample_data_ = nullptr;
  jobject j_output_surface_ = nullptr;

  bool error_occurred_ = false;
  bool audio_only_ = false;
  bool video_only_ = false;
  SbPlayerState player_state_;
  starboard::shared::starboard::media::AudioStreamInfo audio_stream_info_;
  starboard::shared::starboard::media::VideoStreamInfo video_stream_info_;

  int64_t last_media_time_ = 0;
  int64_t seek_time_ = 0;
  int dropped_video_frames_ = 0;
  bool is_playing_ = false;
  bool ended_ = false;

  ErrorCB error_cb_;
  PrerolledCB prerolled_cb_;
  EndedCB ended_cb_;

  Mutex mutex_;
  bool audio_eos_written_ = false;
  bool video_eos_written_ = false;
  bool playback_ended_ = false;
  double playback_rate_ = 0.0;
  bool seeking_ = false;
};

}  // namespace starboard::android::shared::exoplayer

#endif  // STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_BRIDGE_H_
