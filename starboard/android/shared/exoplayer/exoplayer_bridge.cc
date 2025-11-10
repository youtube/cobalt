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
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

#include "cobalt/android/jni_headers/ExoPlayerBridge_jni.h"
#include "cobalt/android/jni_headers/ExoPlayerManager_jni.h"
#include "cobalt/android/jni_headers/ExoPlayerMediaSource_jni.h"

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

}  // namespace

ExoPlayerBridge::ExoPlayerBridge(
    const SbMediaAudioStreamInfo& audio_stream_info,
    const SbMediaVideoStreamInfo& video_stream_info)
    : player_is_destroying_(false),
      playback_error_occurred_(false),
      underflow_(false),
      initialized_(false),
      seeking_(false),
      dropped_frames_(0) {
  JNIEnv* env = AttachCurrentThread();

  if (audio_stream_info.codec != kSbMediaAudioCodecNone) {
    ScopedJavaLocalRef<jobject> j_audio_media_source =
        CreateAudioMediaSource(audio_stream_info);
    if (!j_audio_media_source) {
      SB_LOG(ERROR) << "Could not create ExoPlayer audio MediaSource.";
      return;
    }

    j_audio_media_source_.Reset(j_audio_media_source);
  }

  if (video_stream_info.codec != kSbMediaVideoCodecNone) {
    ScopedJavaLocalRef<jobject> j_video_media_source =
        CreateVideoMediaSource(video_stream_info);
    if (!j_video_media_source) {
      SB_LOG(ERROR) << "Could not create ExoPlayer video MediaSource.";
      return;
    }

    j_video_media_source_.Reset(j_video_media_source);

    ScopedJavaGlobalRef<jobject> j_output_surface(env, AcquireVideoSurface());
    if (!j_output_surface) {
      SB_LOG(ERROR) << "Could not acquire video surface for ExoPlayer.";
      return;
    }
    j_output_surface_.Reset(j_output_surface);
  }

  ScopedJavaLocalRef<jobject> j_exoplayer_manager(
      StarboardBridge::GetInstance()->GetExoPlayerManager(env));
  if (!j_exoplayer_manager) {
    SB_LOG(ERROR) << "Failed to fetch ExoPlayerManager.";
    return;
  }

  j_exoplayer_manager_.Reset(j_exoplayer_manager);

  ScopedJavaLocalRef<jobject> j_exoplayer_bridge =
      Java_ExoPlayerManager_createExoPlayerBridge(
          env, j_exoplayer_manager_, reinterpret_cast<jlong>(this),
          j_audio_media_source_, j_video_media_source_, j_output_surface_,
          false /* prefer_tunnel_mode */);
  if (!j_exoplayer_bridge) {
    SB_LOG(ERROR) << "Could not create Java ExoPlayerBridge.";
    return;
  }

  j_exoplayer_bridge_.Reset(j_exoplayer_bridge);
  j_sample_data_.Reset(env, env->NewByteArray(kMaxSampleBufferSize));
  SB_CHECK(j_sample_data_) << "Failed to allocate |j_sample_data_|.";
}

ExoPlayerBridge::~ExoPlayerBridge() {
  player_is_destroying_.store(true);
  ON_INSTANCE_RELEASED(ExoPlayerBridge);

  if (is_valid()) {
    Java_ExoPlayerManager_destroyExoPlayerBridge(
        AttachCurrentThread(), j_exoplayer_manager_, j_exoplayer_bridge_);
  }
  ClearVideoWindow(true);
  ReleaseVideoSurface();
}

void ExoPlayerBridge::OnSurfaceDestroyed() {
  if (!player_is_destroying_.load()) {
    std::string msg = "ExoPlayer surface is destroyed before playback ended";
    SB_LOG(ERROR) << msg;
    playback_error_occurred_.store(true);
    error_cb_(kSbPlayerErrorDecode, msg);
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
    error_cb_(kSbPlayerErrorDecode, msg.c_str());
    return false;
  }

  return true;
}

bool ExoPlayerBridge::Seek(int64_t timestamp) {
  if (ShouldAbortOperation()) {
    return false;
  }

  Java_ExoPlayerBridge_seek(AttachCurrentThread(), j_exoplayer_bridge_,
                            timestamp);
  underflow_.store(false);
  seeking_.store(true);
  dropped_frames_.store(0);
  return true;
}

bool ExoPlayerBridge::WriteSamples(const InputBuffers& input_buffers,
                                   SbMediaType type) {
  if (ShouldAbortOperation()) {
    return false;
  }

  SB_CHECK_EQ(input_buffers.size(), 1U);

  JNIEnv* env = AttachCurrentThread();

  bool is_key_frame;
  int offset;
  ScopedJavaLocalRef<jobject> j_media_source;
  auto input_buffer = input_buffers.front();

  if (type == kSbMediaTypeAudio) {
    SB_CHECK(j_audio_media_source_);
    is_key_frame = true;
    offset = input_buffer->audio_stream_info().codec == kSbMediaAudioCodecAac
                 ? kADTSHeaderSize
                 : 0;
    j_media_source.Reset(j_audio_media_source_);
  } else {
    SB_CHECK(j_video_media_source_);
    is_key_frame = input_buffer->video_sample_info().is_key_frame;
    offset = 0;
    j_media_source.Reset(j_video_media_source_);
  }

  int size = input_buffer->size() - offset;
  env->SetByteArrayRegion(
      static_cast<jbyteArray>(j_sample_data_.obj()), 0, size,
      reinterpret_cast<const jbyte*>(input_buffer->data() + offset));
  ScopedJavaLocalRef<jbyteArray> sample_data_local_ref(
      env, static_cast<jbyteArray>(env->NewLocalRef(j_sample_data_.obj())));

  Java_ExoPlayerMediaSource_writeSample(
      env, j_media_source, sample_data_local_ref, size,
      input_buffer->timestamp(), is_key_frame);

  return true;
}

bool ExoPlayerBridge::WriteEOS(SbMediaType type) {
  if (ShouldAbortOperation()) {
    return false;
  }

  ScopedJavaLocalRef<jobject> j_media_source;

  if (type == kSbMediaTypeAudio) {
    j_media_source.Reset(j_audio_media_source_);
  } else {
    j_media_source.Reset(j_video_media_source_);
  }

  Java_ExoPlayerMediaSource_writeEndOfStream(AttachCurrentThread(),
                                             j_media_source);
  return true;
}

bool ExoPlayerBridge::SetPause(bool pause) const {
  if (ShouldAbortOperation()) {
    return false;
  }

  JNIEnv* env = AttachCurrentThread();

  if (pause) {
    Java_ExoPlayerBridge_pause(env, j_exoplayer_bridge_);
  } else {
    Java_ExoPlayerBridge_play(env, j_exoplayer_bridge_);
  }

  return true;
}

bool ExoPlayerBridge::SetPlaybackRate(const double playback_rate) const {
  if (ShouldAbortOperation()) {
    return false;
  }
  Java_ExoPlayerBridge_setPlaybackRate(AttachCurrentThread(),
                                       j_exoplayer_bridge_,
                                       static_cast<float>(playback_rate));
  return true;
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
  int64_t media_time_usec = Java_ExoPlayerBridge_getCurrentPositionUs(
      AttachCurrentThread(), j_exoplayer_bridge_);

  return MediaInfo{media_time_usec, dropped_frames_.load(), underflow_.load()};
}

void ExoPlayerBridge::OnInitialized(JNIEnv* env) {
  if (initialized_.exchange(true)) {
    SB_LOG(WARNING) << "ExoPlayer called OnInitialized more than once.";
  }
  initialized_cv_.notify_one();
}

void ExoPlayerBridge::OnBuffering(JNIEnv*) {
  if (!seeking_.load()) {
    underflow_.store(true);
  }
}

void ExoPlayerBridge::OnReady(JNIEnv*) {
  SB_CHECK(prerolled_cb_);
  prerolled_cb_();
  underflow_.store(false);
  seeking_.store(false);
}

void ExoPlayerBridge::OnError(JNIEnv* env, jstring msg) {
  SB_CHECK(error_cb_);
  playback_error_occurred_.store(true);
  SB_LOG(ERROR) << "Reporting error with message "
                << ConvertJavaStringToUTF8(env, msg);
  error_cb_(kSbPlayerErrorDecode, ConvertJavaStringToUTF8(env, msg));
}

void ExoPlayerBridge::OnEnded(JNIEnv*) const {
  SB_CHECK(ended_cb_);
  ended_cb_();
}

void ExoPlayerBridge::OnDroppedVideoFrames(JNIEnv* env, jint count) {
  dropped_frames_.fetch_add(count);
}

bool ExoPlayerBridge::ShouldAbortOperation() const {
  if (playback_error_occurred_.load()) {
    SB_LOG(ERROR)
        << "Aborting ExoPlayer operation after a playback error occurred.";
    return true;
  }
  return false;
}

}  // namespace starboard
