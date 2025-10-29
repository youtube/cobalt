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

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

namespace starboard {

// GENERATED_JAVA_ENUM_PACKAGE: dev.cobalt.media
// GENERATED_JAVA_PREFIX_TO_STRIP: EXOPLAYER_RENDERER_TYPE_
enum ExoPlayerRendererType {
  EXOPLAYER_RENDERER_TYPE_AUDIO,
  EXOPLAYER_RENDERER_TYPE_VIDEO,

  EXOPLAYER_RENDERER_TYPE_MAX = EXOPLAYER_RENDERER_TYPE_VIDEO,
};

// GENERATED_JAVA_ENUM_PACKAGE: dev.cobalt.media
// GENERATED_JAVA_PREFIX_TO_STRIP: EXOPLAYER_AUDIO_CODEC_
enum ExoPlayerAudioCodec {
  EXOPLAYER_AUDIO_CODEC_AAC,
  EXOPLAYER_AUDIO_CODEC_AC3,
  EXOPLAYER_AUDIO_CODEC_EAC3,
  EXOPLAYER_AUDIO_CODEC_OPUS,
  EXOPLAYER_AUDIO_CODEC_VORBIS,
  EXOPLAYER_AUDIO_CODEC_MP3,
  EXOPLAYER_AUDIO_CODEC_FLAC,
  EXOPLAYER_AUDIO_CODEC_PCM,
  EXOPLAYER_AUDIO_CODEC_IAMF,

  EXOPLAYER_AUDIO_CODEC_MAX = EXOPLAYER_AUDIO_CODEC_IAMF,
};

class ExoPlayerBridge final : private VideoSurfaceHolder {
 public:
  struct MediaInfo {
    bool is_playing;
    bool is_eos_played;
    bool is_underflow;
    double playback_rate;
  };

  ExoPlayerBridge(const AudioStreamInfo& audio_stream_info,
                  const VideoStreamInfo& video_stream_info);
  ~ExoPlayerBridge();

  void SetCallbacks(ErrorCB error_cb,
                    PrerolledCB prerolled_cb,
                    EndedCB ended_cb);

  void Seek(int64_t seek_to_timestamp);
  void WriteSamples(const InputBuffers& input_buffers);
  void WriteEndOfStream(SbMediaType stream_type);
  void Play() const;
  void Pause() const;
  void Stop() const;
  void SetVolume(double volume) const;
  void SetPlaybackRate(const double playback_rate);

  int GetDroppedFrames() const;
  int64_t GetCurrentMediaTime(MediaInfo& info) const;

  // Native callbacks.
  void OnInitialized(JNIEnv*);
  void OnReady(JNIEnv*);
  void OnError(JNIEnv* env, jstring error_message);
  void OnBuffering(JNIEnv*);
  void SetPlayingStatus(JNIEnv*, jboolean isPlaying);
  void OnPlaybackEnded(JNIEnv*);

  // VideoSurfaceHolder method.
  void OnSurfaceDestroyed() override;

  bool IsEndOfStreamWritten(SbMediaType type) const {
    std::lock_guard lock(mutex_);
    return type == kSbMediaTypeAudio ? audio_eos_written_ : video_eos_written_;
  }

  bool is_valid() const {
    return !j_exoplayer_bridge_.is_null() && !error_occurred_;
  }

  bool EnsurePlayerIsInitialized();

 private:
  void InitExoplayer();

  base::android::ScopedJavaGlobalRef<jobject> j_exoplayer_manager_;
  base::android::ScopedJavaGlobalRef<jobject> j_exoplayer_bridge_;
  base::android::ScopedJavaGlobalRef<jobject> j_audio_media_source_;
  base::android::ScopedJavaGlobalRef<jobject> j_video_media_source_;
  base::android::ScopedJavaGlobalRef<jobject> j_output_surface_;

  const AudioStreamInfo audio_stream_info_;
  const VideoStreamInfo video_stream_info_;

  int64_t seek_time_ = 0;
  bool is_playing_ = false;
  bool ended_ = false;

  ErrorCB error_cb_;
  PrerolledCB prerolled_cb_;
  EndedCB ended_cb_;

  mutable std::mutex mutex_;
  // Signaled once player initialization is complete.
  std::condition_variable initialized_cv_;  // Guarded by |mutex_|.
  bool initialized_ = false;                // Guarded by |mutex_|.
  bool audio_eos_written_ = false;          // Guarded by |mutex_|.
  bool video_eos_written_ = false;          // Guarded by |mutex_|.
  bool playback_ended_ = false;             // Guarded by |mutex_|.
  double playback_rate_ = 1.0;              // Guarded by |mutex_|.
  bool seeking_ = false;                    // Guarded by |mutex_|.
  bool underflow_ = false;                  // Guarded by |mutex_|.

  std::atomic_bool error_occurred_(false);
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_BRIDGE_H_
