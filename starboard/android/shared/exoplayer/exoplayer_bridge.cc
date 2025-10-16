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

#include <optional>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/check_op.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/media/mime_type.h"

#include "cobalt/android/jni_headers/ExoPlayerBridge_jni.h"
#include "cobalt/android/jni_headers/ExoPlayerManager_jni.h"
#include "cobalt/android/jni_headers/ExoPlayerMediaSource_jni.h"

namespace starboard {
namespace {

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

constexpr int kADTSHeaderSize = 7;
constexpr int kNoOffset = 0;
constexpr int kWaitForInitializedTimeoutUs = 250'000;  // 250 ms.

DECLARE_INSTANCE_COUNTER(ExoPlayerBridge)

std::optional<ExoPlayerAudioCodec> SbAudioCodecToExoPlayerAudioCodec(
    SbMediaAudioCodec codec) {
  switch (codec) {
    case kSbMediaAudioCodecAac:
      return EXOPLAYER_AUDIO_CODEC_AAC;
    case kSbMediaAudioCodecAc3:
      return EXOPLAYER_AUDIO_CODEC_AC3;
    case kSbMediaAudioCodecEac3:
      return EXOPLAYER_AUDIO_CODEC_EAC3;
    case kSbMediaAudioCodecOpus:
      return EXOPLAYER_AUDIO_CODEC_OPUS;
    case kSbMediaAudioCodecVorbis:
      return EXOPLAYER_AUDIO_CODEC_VORBIS;
    case kSbMediaAudioCodecMp3:
      return EXOPLAYER_AUDIO_CODEC_MP3;
    case kSbMediaAudioCodecFlac:
      return EXOPLAYER_AUDIO_CODEC_FLAC;
    case kSbMediaAudioCodecPcm:
      return EXOPLAYER_AUDIO_CODEC_PCM;
    case kSbMediaAudioCodecIamf:
      return EXOPLAYER_AUDIO_CODEC_IAMF;
    default:
      SB_LOG(ERROR) << "ExoPlayerBridge encountered unknown audio codec "
                    << codec;
      return std::nullopt;
  }
}

}  // namespace

ExoPlayerBridge::ExoPlayerBridge(const AudioStreamInfo& audio_stream_info,
                                 const VideoStreamInfo& video_stream_info)
    : audio_stream_info_(audio_stream_info),
      video_stream_info_(video_stream_info) {
  int64_t start_us = CurrentMonotonicTime();
  InitExoplayer();
  int64_t end_us = CurrentMonotonicTime();
  SB_LOG(INFO) << "ExoPlayer init took " << (end_us - start_us) / 1'000
               << " msec.";
  ON_INSTANCE_CREATED(ExoPlayerBridge);
}

ExoPlayerBridge::~ExoPlayerBridge() {
  ON_INSTANCE_RELEASED(ExoPlayerBridge);

  Stop();

  JNIEnv* env = AttachCurrentThread();
  Java_ExoPlayerManager_destroyExoPlayerBridge(env, j_exoplayer_manager_,
                                               j_exoplayer_bridge_);
  ended_ = true;
  ClearVideoWindow(true);
  ReleaseVideoSurface();
}

void ExoPlayerBridge::SetCallbacks(ErrorCB error_cb,
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
}

void ExoPlayerBridge::Seek(int64_t seek_to_timestamp) {
  if (error_occurred_) {
    SB_LOG(ERROR) << "Tried to seek after an error occurred.";
    return;
  }

  Java_ExoPlayerBridge_seek(AttachCurrentThread(), j_exoplayer_bridge_,
                            seek_to_timestamp);

  std::unique_lock<std::mutex> lock(mutex_);
  ended_ = false;
  seek_time_ = seek_to_timestamp;
  playback_ended_ = false;
  audio_eos_written_ = false;
  video_eos_written_ = false;
  seeking_ = true;
  underflow_ = false;
}

void ExoPlayerBridge::WriteSamples(const InputBuffers& input_buffers) {
  SB_CHECK_EQ(input_buffers.size(), 1)
      << "ExoPlayer can only write 1 sample at a time, received "
      << input_buffers.size() << " samples.";

  {
    std::lock_guard lock(mutex_);

    if (ended_) {
      SB_LOG(WARNING) << "Tried to write a sample after playback ended.";
      return;
    }
    seeking_ = false;
  }

  int type = input_buffers.front()->sample_type() == kSbMediaTypeAudio
                 ? kSbMediaTypeAudio
                 : kSbMediaTypeVideo;
  bool is_key_frame = true;

  if (type == kSbMediaTypeVideo) {
    is_key_frame = input_buffers.front()->video_sample_info().is_key_frame;
  }

  bool is_eos = false;

  JNIEnv* env = AttachCurrentThread();
  // Remove the ADTS header from AAC frames before writing to the player.
  int offset = type == kSbMediaTypeAudio &&
                       audio_stream_info_.codec == kSbMediaAudioCodecAac
                   ? kADTSHeaderSize
                   : kNoOffset;
  size_t data_size = input_buffers.front()->size() - offset;
  env->SetByteArrayRegion(
      static_cast<jbyteArray>(j_sample_data_.obj()), 0, data_size,
      reinterpret_cast<const jbyte*>(input_buffers.front()->data() + offset));
  ScopedJavaLocalRef<jbyteArray> media_samples(
      env, static_cast<jbyteArray>(env->NewLocalRef(j_sample_data_.obj())));

  auto media_source =
      type == kSbMediaTypeAudio ? j_audio_media_source_ : j_video_media_source_;
  Java_ExoPlayerMediaSource_writeSample(
      env, media_source, media_samples, data_size,
      input_buffers.front()->timestamp(), is_key_frame, is_eos);
}

void ExoPlayerBridge::WriteEndOfStream(SbMediaType stream_type) {
  auto media_source = stream_type == kSbMediaTypeAudio ? j_audio_media_source_
                                                       : j_video_media_source_;
  Java_ExoPlayerMediaSource_writeEndOfStream(AttachCurrentThread(),
                                             media_source);

  if (stream_type == kSbMediaTypeAudio) {
    audio_eos_written_ = true;
  } else {
    video_eos_written_ = true;
  }
}

void ExoPlayerBridge::Play() const {
  Java_ExoPlayerBridge_play(AttachCurrentThread(), j_exoplayer_bridge_);
}

void ExoPlayerBridge::Pause() const {
  Java_ExoPlayerBridge_pause(AttachCurrentThread(), j_exoplayer_bridge_);
}

void ExoPlayerBridge::Stop() const {
  Java_ExoPlayerBridge_stop(AttachCurrentThread(), j_exoplayer_bridge_);
}

void ExoPlayerBridge::SetVolume(double volume) const {
  Java_ExoPlayerBridge_setVolume(AttachCurrentThread(), j_exoplayer_bridge_,
                                 static_cast<float>(volume));
}

void ExoPlayerBridge::SetPlaybackRate(const double playback_rate) {
  Java_ExoPlayerBridge_setPlaybackRate(AttachCurrentThread(),
                                       j_exoplayer_bridge_,
                                       static_cast<float>(playback_rate));

  std::unique_lock<std::mutex> lock(mutex_);
  playback_rate_ = playback_rate;
}

int ExoPlayerBridge::GetDroppedFrames() const {
  return Java_ExoPlayerBridge_getDroppedFrames(AttachCurrentThread(),
                                               j_exoplayer_bridge_);
}

int64_t ExoPlayerBridge::GetCurrentMediaTime(MediaInfo& info) const {
  {
    std::lock_guard lock(mutex_);
    info.is_playing = is_playing_;
    info.is_eos_played = playback_ended_;
    info.is_underflow = underflow_;
    info.playback_rate = playback_rate_;

    if (seeking_) {
      return seek_time_;
    }
  }
  return Java_ExoPlayerBridge_getCurrentPositionUs(AttachCurrentThread(),
                                                   j_exoplayer_bridge_);
}

void ExoPlayerBridge::OnInitialized(JNIEnv* env) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    initialized_ = true;
  }
  initialized_cv_.notify_one();
}

void ExoPlayerBridge::OnReady(JNIEnv* env) {
  prerolled_cb_();
}

void ExoPlayerBridge::OnError(JNIEnv* env, jstring error_message) {
  SB_CHECK(error_cb_);
  error_cb_(kSbPlayerErrorDecode, ConvertJavaStringToUTF8(env, error_message));
}

void ExoPlayerBridge::OnBuffering(JNIEnv* env) {
  underflow_ = true;
}

void ExoPlayerBridge::SetPlayingStatus(JNIEnv* env, jboolean is_playing) {
  if (error_occurred_) {
    SB_LOG(WARNING) << "Playing status is updated after an error.";
    return;
  }
  is_playing_ = is_playing;

  if (underflow_ && is_playing_) {
    underflow_ = false;
  }
}

void ExoPlayerBridge::OnPlaybackEnded(JNIEnv* env) {
  playback_ended_ = true;
  ended_cb_();
}

bool ExoPlayerBridge::EnsurePlayerIsInitialized() {
  bool wait_timeout;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    wait_timeout = initialized_cv_.wait_for(
        lock, std::chrono::microseconds(kWaitForInitializedTimeoutUs),
        [this] { return initialized_; });
  }

  if (wait_timeout) {
    std::string error_message = starboard::FormatString(
        "ExoPlayer initialization exceeded the %d timeout threshold.",
        kWaitForInitializedTimeoutUs);
    SB_LOG(ERROR) << error_message;
    error_cb_(kSbPlayerErrorDecode, error_message.c_str());
    error_occurred_ = true;
    return false;
  }

  return true;
}

void ExoPlayerBridge::InitExoplayer() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_exoplayer_manager(
      StarboardBridge::GetInstance()->GetExoPlayerManager(env));
  if (!j_exoplayer_manager) {
    SB_LOG(ERROR) << "Failed to fetch ExoPlayerManager.";
    error_occurred_ = true;
    return;
  }

  j_exoplayer_manager_.Reset(j_exoplayer_manager);

  ScopedJavaLocalRef<jobject> j_exoplayer_bridge(
      Java_ExoPlayerManager_createExoPlayerBridge(
          env, j_exoplayer_manager_, reinterpret_cast<jlong>(this),
          /*prefer tunnel mode*/ false));
  if (!j_exoplayer_bridge) {
    SB_LOG(ERROR) << "Failed to create ExoPlayerBridge.";
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
      configuration_data =
          ToJavaByteArray(env, audio_stream_info_.audio_specific_config.data(),
                          audio_stream_info_.audio_specific_config.size());
    }

    bool is_passthrough = audio_stream_info_.codec == kSbMediaAudioCodecEac3 ||
                          audio_stream_info_.codec == kSbMediaAudioCodecAc3;
    std::string j_audio_mime_str = SupportedAudioCodecToMimeType(
        audio_stream_info_.codec, &is_passthrough);
    ScopedJavaLocalRef<jstring> j_audio_mime =
        ConvertUTF8ToJavaString(env, j_audio_mime_str.c_str());

    std::optional<ExoPlayerAudioCodec> exoplayer_codec =
        SbAudioCodecToExoPlayerAudioCodec(audio_stream_info_.codec);
    if (!exoplayer_codec.has_value()) {
      error_occurred_ = true;
      SB_LOG(ERROR) << "Could not create ExoPlayer audio media source.";
      return;
    }

    ScopedJavaLocalRef<jobject> j_audio_media_source(
        Java_ExoPlayerBridge_createAudioMediaSource(
            env, j_exoplayer_bridge_, exoplayer_codec.value(), j_audio_mime,
            configuration_data, samplerate, channels, bits_per_sample));
    if (!j_audio_media_source) {
      SB_LOG(ERROR) << "Could not create ExoPlayer audio media source.";
      error_occurred_ = true;
      return;
    }
    j_audio_media_source_.Reset(j_audio_media_source);
  }

  if (video_stream_info_.codec != kSbMediaVideoCodecNone) {
    ScopedJavaGlobalRef<jobject> j_output_surface(env, AcquireVideoSurface());
    if (!j_output_surface) {
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
      framerate = mime_type.GetParamIntValue("framerate", 0);
      bitrate = mime_type.GetParamIntValue("bitrate", 0);
    }

    ScopedJavaLocalRef<jobject> j_video_media_source(
        Java_ExoPlayerBridge_createVideoMediaSource(
            env, j_exoplayer_bridge_, j_mime, j_output_surface_, width, height,
            framerate, bitrate));
    if (!j_video_media_source) {
      SB_LOG(ERROR) << "Could not create ExoPlayer video MediaSource.";
      error_occurred_ = true;
      return;
    }
    j_video_media_source_.Reset(j_video_media_source);
  }

  j_sample_data_.Reset(env->NewByteArray(2 * 65536 * 32));
  SB_CHECK(j_sample_data_) << "Failed to allocate |j_sample_data_|";

  Java_ExoPlayerBridge_preparePlayer(env, j_exoplayer_bridge_);
}

}  // namespace starboard
