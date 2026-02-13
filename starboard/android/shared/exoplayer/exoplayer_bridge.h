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
#include "starboard/shared/starboard/thread_checker.h"

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

  void Seek(int64_t timestamp);
  void WriteSamples(const InputBuffers& input_buffers, SbMediaType type);
  void WriteEOS(SbMediaType type) const;
  void SetPause(bool pause) const;
  void SetPlaybackRate(const double playback_rate) const;
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

  bool is_valid() const { return !j_exoplayer_bridge_.is_null(); }

  std::string GetInitErrorMessage() const { return init_error_msg_; }

 private:
  bool ShouldAbortOperation() const;
  void ReportError(const std::string& msg) const;

  base::android::ScopedJavaGlobalRef<jobject> j_exoplayer_bridge_;

  std::atomic_bool player_is_releasing_ = false;

  // The following variables may be accessed by the ExoPlayer Looper
  // thread.
  std::atomic_bool playback_error_occurred_ = false;
  std::atomic_bool initialized_ = false;
  std::atomic_bool seeking_ = false;
  std::atomic_bool is_playing_ = false;
  std::atomic_int32_t dropped_frames_ = 0;

  ErrorCB error_cb_;
  PrerolledCB prerolled_cb_;
  EndedCB ended_cb_;

  std::mutex mutex_;
  std::condition_variable initialized_cv_;  // Guarded by |mutex_|.

  bool owns_surface_ = false;
  std::string init_error_msg_;

  ThreadChecker thread_checker_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_BRIDGE_H_
