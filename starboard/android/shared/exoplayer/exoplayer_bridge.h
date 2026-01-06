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

#include <jni.h>

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

namespace starboard {

// GENERATED_JAVA_ENUM_PACKAGE: dev.cobalt.media
// GENERATED_JAVA_PREFIX_TO_STRIP: EXOPLAYER_RENDERER_TYPE_
enum ExoPlayerRendererType {
  EXOPLAYER_RENDERER_TYPE_AUDIO = 0,  // Must match kSbMediaTypeAudio.
  EXOPLAYER_RENDERER_TYPE_VIDEO = 1,  // Must match kSbMediaTypeVideo.

  EXOPLAYER_RENDERER_TYPE_MAX = EXOPLAYER_RENDERER_TYPE_VIDEO,
};

class ExoPlayerBridge final : private VideoSurfaceHolder {
 public:
  struct MediaInfo {
    int64_t media_time_usec;
    int dropped_frames;
    bool is_playing;
  };

  ExoPlayerBridge(const SbMediaAudioStreamInfo& audio_stream_info,
                  const SbMediaVideoStreamInfo& video_stream_info);
  ~ExoPlayerBridge();

  // VideoSurfaceHolder method.
  void OnSurfaceDestroyed() override;

  bool Init(ErrorCB error_cb, PrerolledCB prerolled_cb, EndedCB ended_cb);

  bool Seek(int64_t timestamp);
  bool WriteSamples(const InputBuffers& input_buffers, SbMediaType type);
  bool WriteEOS(SbMediaType type) const;
  bool SetPause(bool pause) const;
  bool SetPlaybackRate(const double playback_rate) const;
  void SetVolume(const double volume) const;
  void Stop() const;

  MediaInfo GetMediaInfo() const;
  bool CanAcceptMoreData(SbMediaType type) const;

  // Native callbacks.
  void OnInitialized(JNIEnv*);
  void OnReady(JNIEnv*);
  void OnError(JNIEnv* env, jstring msg);
  void OnEnded(JNIEnv*) const;
  void OnDroppedVideoFrames(JNIEnv* env, jint count);
  void OnIsPlayingChanged(JNIEnv*, jboolean is_playing);

  bool is_valid() const { return j_exoplayer_manager_ && j_exoplayer_bridge_; }

 private:
  bool ShouldAbortOperation() const;
  void ReportError(const std::string& msg) const;

  base::android::ScopedJavaGlobalRef<jobject> j_exoplayer_manager_;
  base::android::ScopedJavaGlobalRef<jobject> j_exoplayer_bridge_;
  base::android::ScopedJavaGlobalRef<jobject> j_sample_data_;

  std::atomic_bool player_is_destroying_;
  std::atomic_bool playback_error_occurred_;
  std::atomic_bool initialized_;
  std::atomic_bool seeking_;
  std::atomic_bool is_playing_;
  std::atomic_int32_t dropped_frames_;

  ErrorCB error_cb_;
  PrerolledCB prerolled_cb_;
  EndedCB ended_cb_;

  std::mutex mutex_;
  std::condition_variable initialized_cv_;  // Guarded by |mutex_|.

  bool owns_surface_;
  std::string init_error_msg_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_BRIDGE_H_
