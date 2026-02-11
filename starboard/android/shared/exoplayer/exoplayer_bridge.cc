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

#include "starboard/android/shared/exoplayer/exoplayer_bridge.h"

#include <jni.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "starboard/android/shared/exoplayer/exoplayer_util.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/check_op.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

#include "cobalt/android/jni_headers/ExoPlayerBridge_jni.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "cobalt/android/jni_headers/ExoPlayerManager_jni.h"
#pragma GCC diagnostic pop

namespace starboard {
namespace {

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

DECLARE_INSTANCE_COUNTER(ExoPlayerBridge)

constexpr int kWaitForInitializedTimeoutUsec = 250'000;  // 250 ms.
constexpr int kMaxSampleBufferSize = 6 * 1024 * 1024;    // 6 MB.
constexpr int kADTSHeaderSize = 7;
constexpr bool kForceTunneledPlayback = false;

int GetSampleOffset(SbMediaType type, scoped_refptr<InputBuffer> input_buffer) {
  if (type == kSbMediaTypeAudio &&
      input_buffer->audio_stream_info().codec == kSbMediaAudioCodecAac) {
    return kADTSHeaderSize;
  }
  return 0;
}

}  // namespace

ExoPlayerBridge::ExoPlayerBridge(
    const SbMediaAudioStreamInfo& audio_stream_info,
    const SbMediaVideoStreamInfo& video_stream_info) {
  ON_INSTANCE_CREATED(ExoPlayerBridge);

  ScopedJavaLocalRef<jobject> j_audio_media_source;
  if (audio_stream_info.codec != kSbMediaAudioCodecNone) {
    j_audio_media_source = CreateAudioMediaSource(audio_stream_info);
    if (!j_audio_media_source) {
      init_error_msg_ = "Could not create ExoPlayer audio MediaSource";
      SB_LOG(ERROR) << init_error_msg_;
      return;
    }
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_video_media_source;
  ScopedJavaGlobalRef<jobject> j_output_surface;
  if (video_stream_info.codec != kSbMediaVideoCodecNone) {
    j_video_media_source = CreateVideoMediaSource(video_stream_info);
    if (!j_video_media_source) {
      init_error_msg_ = "Could not create ExoPlayer video MediaSource";
      SB_LOG(ERROR) << init_error_msg_;
      return;
    }

    j_output_surface.Reset(env, AcquireVideoSurface());
    if (!j_output_surface) {
      init_error_msg_ = "Could not acquire video surface for ExoPlayer";
      SB_LOG(ERROR) << init_error_msg_;
      return;
    }
    owns_surface_ = true;
  }

  ScopedJavaLocalRef<jobject> j_exoplayer_manager(
      StarboardBridge::GetInstance()->GetExoPlayerManager(env));
  if (!j_exoplayer_manager) {
    init_error_msg_ = "Failed to fetch ExoPlayerManager";
    SB_LOG(ERROR) << init_error_msg_;
    return;
  }

  ScopedJavaLocalRef<jobject> j_exoplayer_bridge =
      Java_ExoPlayerManager_createExoPlayerBridge(
          env, j_exoplayer_manager, reinterpret_cast<jlong>(this),
          j_audio_media_source, j_video_media_source, j_output_surface,
          (video_stream_info.codec != kSbMediaVideoCodecNone &&
           (audio_stream_info.codec == kSbMediaAudioCodecAc3 ||
            audio_stream_info.codec == kSbMediaAudioCodecEac3) &&
           ShouldEnableTunneledPlayback(video_stream_info)) ||
              kForceTunneledPlayback);
  if (!j_exoplayer_bridge) {
    init_error_msg_ = "Could not create Java ExoPlayerBridge";
    SB_LOG(ERROR) << init_error_msg_;
    return;
  }

  j_exoplayer_bridge_.Reset(j_exoplayer_bridge);
  j_sample_data_.Reset(env, env->NewByteArray(kMaxSampleBufferSize));
  SB_CHECK(j_sample_data_) << "Failed to allocate |j_sample_data_|.";
}

ExoPlayerBridge::~ExoPlayerBridge() {
  ON_INSTANCE_RELEASED(ExoPlayerBridge);
  player_is_releasing_.store(true);

  if (is_valid()) {
    Java_ExoPlayerBridge_release(AttachCurrentThread(), j_exoplayer_bridge_);
  }

  if (owns_surface_) {
    ClearVideoWindow(true);
    ReleaseVideoSurface();
  }
}

void ExoPlayerBridge::OnSurfaceDestroyed() {
  if (!player_is_releasing_.load()) {
    std::string msg = "ExoPlayer surface is destroyed before playback ended";
    SB_LOG(ERROR) << msg;
    playback_error_occurred_.store(true);
    ReportError(msg);
  }
}

bool ExoPlayerBridge::Init(ErrorCB error_cb,
                           PrerolledCB prerolled_cb,
                           EndedCB ended_cb) {
  SB_CHECK(error_cb);
  SB_CHECK(prerolled_cb);
  SB_CHECK(ended_cb);
  SB_CHECK(!error_cb_);
  SB_CHECK(!prerolled_cb_);
  SB_CHECK(!ended_cb_);

  error_cb_ = std::move(error_cb);
  prerolled_cb_ = std::move(prerolled_cb);
  ended_cb_ = std::move(ended_cb);

  bool completed_init;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    completed_init = initialized_cv_.wait_for(
        lock, std::chrono::microseconds(kWaitForInitializedTimeoutUsec),
        [this] { return initialized_.load(); });
  }

  if (!completed_init) {
    std::string msg = "ExoPlayer failed to initialize in time";
    SB_LOG(ERROR) << msg;
    ReportError(msg);
    return false;
  }

  return true;
}

void ExoPlayerBridge::Seek(int64_t timestamp) {
  if (ShouldAbortOperation()) {
    return;
  }

  seeking_.store(true);
  Java_ExoPlayerBridge_seek(AttachCurrentThread(), j_exoplayer_bridge_,
                            timestamp);
}

void ExoPlayerBridge::WriteSamples(const InputBuffers& input_buffers,
                                   SbMediaType type) {
  // TODO: It's possible that a video sample may contain valid
  // SbMediaColorMetadata after codec creation. When that happens,
  // we should recreate the video decoder to use the new metadata.
  if (ShouldAbortOperation()) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();

  for (auto& input_buffer : input_buffers) {
    bool is_key_frame = type == kSbMediaTypeAudio
                            ? true
                            : input_buffer->video_sample_info().is_key_frame;
    int offset = GetSampleOffset(type, input_buffer);
    int size = input_buffer->size() - offset;

    env->SetByteArrayRegion(
        static_cast<jbyteArray>(j_sample_data_.obj()), 0, size,
        reinterpret_cast<const jbyte*>(input_buffer->data() + offset));
    ScopedJavaLocalRef<jbyteArray> sample_data_local_ref(
        env, static_cast<jbyteArray>(env->NewLocalRef(j_sample_data_.obj())));

    Java_ExoPlayerBridge_writeSample(
        env, j_exoplayer_bridge_, sample_data_local_ref, size,
        input_buffer->timestamp(), is_key_frame, type);
  }
}

void ExoPlayerBridge::WriteEOS(SbMediaType type) const {
  if (ShouldAbortOperation()) {
    return;
  }

  Java_ExoPlayerBridge_writeEndOfStream(AttachCurrentThread(),
                                        j_exoplayer_bridge_, type);
}

void ExoPlayerBridge::SetPause(bool pause) const {
  if (ShouldAbortOperation()) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();

  if (pause) {
    Java_ExoPlayerBridge_pause(env, j_exoplayer_bridge_);
  } else {
    Java_ExoPlayerBridge_play(env, j_exoplayer_bridge_);
  }
}

void ExoPlayerBridge::SetPlaybackRate(const double playback_rate) const {
  if (ShouldAbortOperation()) {
    return;
  }

  Java_ExoPlayerBridge_setPlaybackRate(AttachCurrentThread(),
                                       j_exoplayer_bridge_,
                                       static_cast<float>(playback_rate));
}

void ExoPlayerBridge::SetVolume(const double volume) const {
  if (ShouldAbortOperation()) {
    return;
  }

  Java_ExoPlayerBridge_setVolume(AttachCurrentThread(), j_exoplayer_bridge_,
                                 static_cast<float>(volume));
}

void ExoPlayerBridge::Stop() const {
  if (ShouldAbortOperation()) {
    return;
  }

  Java_ExoPlayerBridge_stop(AttachCurrentThread(), j_exoplayer_bridge_);
}

ExoPlayerBridge::MediaInfo ExoPlayerBridge::GetMediaInfo() const {
  if (ShouldAbortOperation()) {
    return MediaInfo();
  }

  int64_t media_time_usec = Java_ExoPlayerBridge_getCurrentPositionUsec(
      AttachCurrentThread(), j_exoplayer_bridge_);

  return MediaInfo{media_time_usec, dropped_frames_.load(), is_playing_.load()};
}

bool ExoPlayerBridge::CanAcceptMoreData(SbMediaType type) const {
  if (ShouldAbortOperation()) {
    return false;
  }
  return Java_ExoPlayerBridge_canAcceptMoreData(AttachCurrentThread(),
                                                j_exoplayer_bridge_, type);
}

void ExoPlayerBridge::OnInitialized(JNIEnv*) {
  if (initialized_.exchange(true)) {
    SB_LOG(WARNING)
        << "ExoPlayer called OnInitialized again after initialization.";
  }
  std::lock_guard lock(mutex_);
  initialized_cv_.notify_one();
}

void ExoPlayerBridge::OnReady(JNIEnv*) {
  if (seeking_.exchange(false)) {
    SB_CHECK(prerolled_cb_);
    prerolled_cb_();
  }
}

void ExoPlayerBridge::OnError(JNIEnv* env, jstring msg) {
  playback_error_occurred_.store(true);
  std::string error_msg = ConvertJavaStringToUTF8(env, msg);
  SB_LOG(ERROR) << "Reporting playback error: " << error_msg;
  ReportError(error_msg);
}

void ExoPlayerBridge::OnEnded(JNIEnv*) const {
  SB_CHECK(ended_cb_);
  ended_cb_();
}

void ExoPlayerBridge::OnDroppedVideoFrames(JNIEnv* env, jint count) {
  dropped_frames_.fetch_add(count);
}

void ExoPlayerBridge::OnIsPlayingChanged(JNIEnv*, jboolean is_playing) {
  is_playing_.store(is_playing);
}

bool ExoPlayerBridge::ShouldAbortOperation() const {
  if (playback_error_occurred_.load()) {
    SB_LOG(ERROR)
        << "Aborting ExoPlayer operation after a playback error occurred.";
    return true;
  }
  return false;
}

void ExoPlayerBridge::ReportError(const std::string& msg) const {
  SB_CHECK(error_cb_);
  error_cb_(kSbPlayerErrorDecode, msg);
}

}  // namespace starboard
