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
#include <unistd.h>

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/media/mime_type.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/CobaltMediaSource_jni.h"
#include "cobalt/android/jni_headers/ExoPlayerBridge_jni.h"
#include "cobalt/android/jni_headers/ExoPlayerManager_jni.h"

namespace starboard::android::shared::exoplayer {
namespace {

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

constexpr int kADTSHeaderSize = 7;

// TODO: For debug, remove this
#if defined(COBALT_BUILD_TYPE_GOLD) || defined(COBALT_BUILD_TYPE_QA)
constexpr int kWaitForInitializedTimeout = 7'000'000;  // 700 ms
#else
constexpr int kWaitForInitializedTimeout = 10'000'000;  // 1000 ms
#endif

DECLARE_INSTANCE_COUNTER(ExoPlayerBridge)

// Matches the values in the Android Player implementation.
constexpr jint PLAYER_STATE_IDLE = 1;
constexpr jint PLAYER_STATE_BUFFERING = 2;
constexpr jint PLAYER_STATE_READY = 3;
constexpr jint PLAYER_STATE_ENDED = 4;
}  // namespace

ExoPlayerBridge::ExoPlayerBridge(const AudioStreamInfo& audio_stream_info,
                                 const VideoStreamInfo& video_stream_info)
    : audio_stream_info_(audio_stream_info),
      video_stream_info_(video_stream_info) {
  int64_t start = CurrentMonotonicTime();
  InitExoplayer();
  int64_t end = CurrentMonotonicTime();
  SB_LOG(INFO) << "ExoPlayer init took " << end - start << "us";
  ON_INSTANCE_CREATED(ExoPlayerBridge);
}

ExoPlayerBridge::~ExoPlayerBridge() {
  ON_INSTANCE_RELEASED(ExoPlayerBridge);

  int64_t start = CurrentMonotonicTime();
  TearDownExoPlayer();
  int64_t end = CurrentMonotonicTime();
  SB_LOG(INFO) << "ExoPlayer tear down took " << end - start << "us";
}

void ExoPlayerBridge::SetCallbacks(const ErrorCB& error_cb,
                                   const PrerolledCB& prerolled_cb,
                                   const EndedCB& ended_cb) {
  SB_CHECK(error_cb);
  SB_CHECK(prerolled_cb);
  SB_CHECK(ended_cb);
  SB_CHECK(!error_cb_);
  SB_CHECK(!prerolled_cb_);
  SB_CHECK(!ended_cb_);

  error_cb_ = error_cb;
  prerolled_cb_ = prerolled_cb;
  ended_cb_ = ended_cb;
}

void ExoPlayerBridge::Seek(int64_t seek_to_timestamp) {
  if (error_occurred_) {
    SB_LOG(ERROR) << "Tried to seek after an error occurred.";
    return;
  }

  SB_DLOG(INFO) << "Trying to seek to " << seek_to_timestamp << "microseconds.";

  if (!Java_ExoPlayerBridge_seek(AttachCurrentThread(), j_exoplayer_bridge_,
                                 seek_to_timestamp)) {
    SB_LOG(INFO) << "ExoPlayer failed seek.";
    error_occurred_ = true;
    error_cb_(kSbPlayerErrorDecode, "ExoPlayer ailed seek");
    return;
  }

  std::unique_lock<std::mutex> lock(mutex_);
  ended_ = false;
  seek_time_ = seek_to_timestamp;
  playback_ended_ = false;
  audio_eos_written_ = false;
  video_eos_written_ = false;
  seeking_ = true;
}

void ExoPlayerBridge::WriteSamples(const InputBuffers& input_buffers) {
  SB_CHECK(!input_buffers.empty());
  SB_CHECK(input_buffers.size() == 1)
      << "ExoPlayer can only write 1 sample at a time, received "
      << input_buffers.size() << " samples.";

  {
    std::unique_lock<std::mutex> lock(mutex_);

    if (ended_) {
      SB_LOG(WARNING) << "Tried to write a sample after playback ended";
      return;
    }
    seeking_ = false;
  }

  int type = input_buffers.front()->sample_type() == kSbMediaTypeAudio
                 ? EXOPLAYER_RENDERER_TYPE_AUDIO
                 : EXOPLAYER_RENDERER_TYPE_VIDEO;
  bool is_key_frame = true;

  if (type == EXOPLAYER_RENDERER_TYPE_VIDEO) {
    is_key_frame = input_buffers.front()->video_sample_info().is_key_frame;
    if (is_key_frame) {
      SB_LOG(INFO) << "AUSTIN: VIDEO FRAME IS A KEY FRAME";
    }
  }

  bool is_eos = false;

  JNIEnv* env = AttachCurrentThread();
  size_t data_size = input_buffers.front()->size();
  if (type == EXOPLAYER_RENDERER_TYPE_AUDIO &&
      audio_stream_info_.codec == kSbMediaAudioCodecAac) {
    SB_LOG(INFO) << "Truncating sample for aac, config size is "
                 << audio_stream_info_.audio_specific_config.size();
    ;
    j_sample_data_ = ScopedJavaLocalRef(
        ToJavaByteArray(env, input_buffers.front()->data() + kADTSHeaderSize,
                        input_buffers.front()->size() - kADTSHeaderSize));
    data_size -= kADTSHeaderSize;
  } else {
    j_sample_data_ = ScopedJavaLocalRef(ToJavaByteArray(
        env, input_buffers.front()->data(), input_buffers.front()->size()));
  }

  auto media_source =
      type == kSbMediaTypeAudio ? j_audio_media_source_ : j_video_media_source_;
  SB_LOG(INFO) << "Writing " << (type == kSbMediaTypeAudio ? "audio" : "video")
               << " sample with timestamp "
               << input_buffers.front()->timestamp();
  Java_CobaltMediaSource_writeSample(
      env, media_source, j_sample_data_, data_size,
      input_buffers.front()->timestamp(), is_key_frame, is_eos);
}

void ExoPlayerBridge::WriteEndOfStream(SbMediaType stream_type) {
  int type = stream_type == kSbMediaTypeAudio ? EXOPLAYER_RENDERER_TYPE_AUDIO
                                              : EXOPLAYER_RENDERER_TYPE_VIDEO;
  JNIEnv* env = AttachCurrentThread();
  uint8_t byte = 0xef;
  int64_t timestamp = 0;
  int buffer_size = 1;
  bool is_key_frame = false;
  bool is_eos = true;
  j_sample_data_ = ToJavaByteArray(env, &byte, buffer_size);
  auto media_source =
      type == kSbMediaTypeAudio ? j_audio_media_source_ : j_video_media_source_;
  Java_CobaltMediaSource_writeEndOfStream(AttachCurrentThread(), media_source);
  // Java_ExoPlayerBridge_writeSample(env, j_exoplayer_bridge_, j_sample_data_,
  //                                  buffer_size, timestamp, type,
  //                                  is_key_frame, is_eos);

  if (stream_type == kSbMediaTypeAudio) {
    audio_eos_written_ = true;
  } else {
    video_eos_written_ = true;
  }
}

bool ExoPlayerBridge::Play() {
  if (!Java_ExoPlayerBridge_play(AttachCurrentThread(), j_exoplayer_bridge_)) {
    error_cb_(kSbPlayerErrorDecode, "ExoPlayer play() failed.");
    return false;
  }

  return true;
}

bool ExoPlayerBridge::Pause() {
  if (!Java_ExoPlayerBridge_pause(AttachCurrentThread(), j_exoplayer_bridge_)) {
    error_cb_(kSbPlayerErrorDecode, "ExoPlayer pause() failed");
    return false;
  }
  return true;
}

bool ExoPlayerBridge::Stop() {
  if (!Java_ExoPlayerBridge_stop(AttachCurrentThread(), j_exoplayer_bridge_)) {
    error_cb_(kSbPlayerErrorDecode, "ExoPlayer stop() failed.");
  }
  return true;
}

bool ExoPlayerBridge::SetVolume(double volume) {
  if (!Java_ExoPlayerBridge_setVolume(AttachCurrentThread(),
                                      j_exoplayer_bridge_,
                                      static_cast<float>(volume))) {
    error_cb_(kSbPlayerErrorDecode, "ExoPlayer setVolume() failed.");
    return false;
  }
  return true;
}

bool ExoPlayerBridge::SetPlaybackRate(const double playback_rate) {
  if (!Java_ExoPlayerBridge_setPlaybackRate(
          AttachCurrentThread(), j_exoplayer_bridge_,
          static_cast<float>(playback_rate))) {
    error_cb_(kSbPlayerErrorDecode, "ExoPlayer setPlaybackRate() failed.");
    return false;
  }

  std::unique_lock<std::mutex> lock(mutex_);
  playback_rate_ = playback_rate;

  return true;
}

int ExoPlayerBridge::GetDroppedFrames() {
  JNIEnv* env = AttachCurrentThread();
  jint dropped_frames =
      Java_ExoPlayerBridge_getDroppedFrames(env, j_exoplayer_bridge_);
  return dropped_frames;
}

int64_t ExoPlayerBridge::GetCurrentMediaTime(MediaInfo& info) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    info.is_playing = is_playing_;
    info.is_eos_played = playback_ended_;
    // TODO: Report underflow when buffering after playback start
    info.is_underflow = false;
    info.playback_rate = playback_rate_;

    if (seeking_) {
      return seek_time_;
    }
  }
  return Java_ExoPlayerBridge_getCurrentPositionUs(AttachCurrentThread(),
                                                   j_exoplayer_bridge_);
}

void ExoPlayerBridge::OnPlaybackStateChanged(JNIEnv* env, jint playback_state) {
  // TODO: Remove debug logs
  if (playback_state == PLAYER_STATE_IDLE) {
    SB_LOG(INFO) << "Player is IDLE";
  } else if (playback_state == PLAYER_STATE_BUFFERING) {
    SB_LOG(INFO) << "Player is BUFFERING";
  } else if (playback_state == PLAYER_STATE_READY) {
    SB_LOG(INFO) << "Player is READY";
  } else if (playback_state == PLAYER_STATE_ENDED) {
    SB_LOG(INFO) << "Player has ENDED";
    OnPlaybackEnded();
  }
}

void ExoPlayerBridge::OnInitialized(JNIEnv* env) {
  std::unique_lock<std::mutex> lock(mutex_);
  initialized_cv_.notify_one();
}

void ExoPlayerBridge::OnReady(JNIEnv* env) {
  prerolled_cb_();
}

void ExoPlayerBridge::OnError(JNIEnv* env, jstring error_message) {
  SB_CHECK(error_cb_);
  error_cb_(kSbPlayerErrorDecode, ConvertJavaStringToUTF8(env, error_message));
}

void ExoPlayerBridge::SetPlayingStatus(JNIEnv* env, jboolean is_playing) {
  if (error_occurred_) {
    SB_LOG(WARNING) << "Playing status is updated after an error.";
    return;
  }
  SB_LOG(INFO) << "Changing is_playing_ to " << is_playing;
  is_playing_ = is_playing;
}

void ExoPlayerBridge::OnPlaybackEnded() {
  playback_ended_ = true;
  ended_cb_();
}

bool ExoPlayerBridge::EnsurePlayerIsInitialized() {
  bool wait_timeout;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    wait_timeout =
        initialized_cv_.wait_for(
            lock, std::chrono::microseconds(kWaitForInitializedTimeout)) ==
        std::cv_status::timeout;
  }

  if (wait_timeout) {
    SB_LOG(ERROR) << "ExoPlayer initialization took too long";
    error_cb_(kSbPlayerErrorDecode, "ExoPlayer initialization took too long");
    return false;
  }
  return true;
}

void ExoPlayerBridge::InitExoplayer() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_exoplayer_manager(
      StarboardBridge::GetInstance()->GetExoPlayerManager(env));
  if (j_exoplayer_manager.is_null()) {
    SB_LOG(WARNING) << "Failed to fetch ExoPlayerManager.";
    error_occurred_ = true;
    return;
  }
  j_exoplayer_manager_.Reset(j_exoplayer_manager);

  ScopedJavaLocalRef<jobject> j_exoplayer_bridge(
      Java_ExoPlayerManager_createExoPlayerBridge(
          env, j_exoplayer_manager_, reinterpret_cast<jlong>(this),
          /*prefer tunnel mode*/ false));
  if (j_exoplayer_bridge.is_null()) {
    SB_LOG(WARNING) << "Failed to create ExoPlayerBridge.";
    error_occurred_ = true;
    return;
  }
  j_exoplayer_bridge_.Reset(j_exoplayer_bridge);

  if (audio_stream_info_.codec != kSbMediaAudioCodecNone) {
    int samplerate = audio_stream_info_.samples_per_second;
    int channels = audio_stream_info_.number_of_channels;
    int bits_per_sample = audio_stream_info_.bits_per_sample;

    ScopedJavaLocalRef<jbyteArray> configuration_data;
    if (!audio_stream_info_.audio_specific_config.empty()) {
      SB_LOG(INFO) << "Setting config data for codec "
                   << audio_stream_info_.codec;
      configuration_data =
          ToJavaByteArray(env, audio_stream_info_.audio_specific_config.data(),
                          audio_stream_info_.audio_specific_config.size());
    }

    // TODO: Test on-device E/AC3 decoding.
    bool is_passthrough = audio_stream_info_.codec == kSbMediaAudioCodecEac3 ||
                          audio_stream_info_.codec == kSbMediaAudioCodecAc3;
    std::string j_audio_mime_str = SupportedAudioCodecToMimeType(
        audio_stream_info_.codec, &is_passthrough);
    ScopedJavaLocalRef<jstring> j_audio_mime =
        ConvertUTF8ToJavaString(env, j_audio_mime_str.c_str());

    ScopedJavaLocalRef<jobject> j_audio_media_source(
        Java_ExoPlayerBridge_createAudioMediaSource(
            env, j_exoplayer_bridge_, j_audio_mime, configuration_data,
            samplerate, channels, bits_per_sample));
    if (j_audio_media_source.is_null()) {
      SB_LOG(ERROR) << "Could not create ExoPlayer audio media source.";
      error_occurred_ = true;
      return;
    }
    j_audio_media_source_.Reset(j_audio_media_source);
  }

  if (video_stream_info_.codec != kSbMediaVideoCodecNone) {
    ScopedJavaGlobalRef<jobject> j_output_surface(env, AcquireVideoSurface());
    if (j_output_surface.is_null()) {
      SB_LOG(ERROR) << "Could not acquire video surface for ExoPlayer.";
      error_occurred_ = true;
      return;
    }
    j_output_surface_.Reset(j_output_surface);

    int width = video_stream_info_.codec != kSbMediaVideoCodecNone
                    ? video_stream_info_.frame_size.width
                    : 0;
    int height = video_stream_info_.codec != kSbMediaVideoCodecNone
                     ? video_stream_info_.frame_size.height
                     : 0;
    int framerate = 0;
    int bitrate = 0;

    std::string mime_str =
        SupportedVideoCodecToMimeType(video_stream_info_.codec);
    ScopedJavaLocalRef<jstring> j_mime(
        ConvertUTF8ToJavaString(env, mime_str.c_str()));

    starboard::shared::starboard::media::MimeType mime_type(
        video_stream_info_.mime);
    if (mime_type.is_valid()) {
      framerate = mime_type.GetParamIntValue("framerate", 30);
      bitrate = mime_type.GetParamIntValue("bitrate", 0);
    }

    ScopedJavaLocalRef<jobject> j_video_media_source(
        Java_ExoPlayerBridge_createVideoMediaSource(
            env, j_exoplayer_bridge_, j_mime, j_output_surface_, width, height,
            framerate, bitrate));
    if (j_video_media_source.is_null()) {
      SB_LOG(ERROR) << "Could not create ExoPlayer video MediaSource.";
      error_occurred_ = true;
      return;
    }
    j_video_media_source_.Reset(j_video_media_source);
  }

  j_sample_data_.Reset(ToJavaByteArray(
      env, std::vector<uint8_t>(2 * 65536 * 32, 0).data(), 2 * 65536 * 32));
  SB_CHECK(j_sample_data_) << "Failed to allocate |j_sample_data_|";

  // Prepare the player to receive samples asynchronously.
  Java_ExoPlayerBridge_preparePlayer(env, j_exoplayer_bridge_);
}

void ExoPlayerBridge::TearDownExoPlayer() {
  Stop();

  JNIEnv* env = AttachCurrentThread();
  Java_ExoPlayerManager_destroyExoPlayerBridge(env, j_exoplayer_manager_,
                                               j_exoplayer_bridge_);
  ended_ = true;
  ClearVideoWindow(true);
  ReleaseVideoSurface();
}

}  // namespace starboard::android::shared::exoplayer
