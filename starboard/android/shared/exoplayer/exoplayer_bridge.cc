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
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/media/mime_type.h"

#include "cobalt/android/jni_headers/ExoPlayerBridge_jni.h"
#include "cobalt/android/jni_headers/ExoPlayerFormatCreator_jni.h"
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
const SbMediaMasteringMetadata kEmptyMasteringMetadata = {};

constexpr jint COLOR_RANGE_FULL = 1;
constexpr jint COLOR_RANGE_LIMITED = 2;
// Not defined in MediaFormat. Represents unspecified color ID range.
constexpr jint COLOR_RANGE_UNSPECIFIED = 0;

constexpr jint COLOR_STANDARD_BT2020 = 6;
constexpr jint COLOR_STANDARD_BT709 = 1;

constexpr jint COLOR_TRANSFER_HLG = 7;
constexpr jint COLOR_TRANSFER_SDR_VIDEO = 3;
constexpr jint COLOR_TRANSFER_ST2084 = 6;

// A special value to represent that no mapping between an SbMedia* HDR
// metadata value and Android HDR metadata value is possible.  This value
// implies that HDR playback should not be attempted.
constexpr jint COLOR_VALUE_UNKNOWN = -1;

DECLARE_INSTANCE_COUNTER(ExoPlayerBridge)

// From //starboard/android/shared/video_decoder.cc.
bool Equal(const SbMediaMasteringMetadata& lhs,
           const SbMediaMasteringMetadata& rhs) {
  return memcmp(&lhs, &rhs, sizeof(SbMediaMasteringMetadata)) == 0;
}

bool IsIdentity(const SbMediaColorMetadata& color_metadata) {
  return color_metadata.primaries == kSbMediaPrimaryIdBt709 &&
         color_metadata.transfer == kSbMediaTransferIdBt709 &&
         color_metadata.matrix == kSbMediaMatrixIdBt709 &&
         color_metadata.range == kSbMediaRangeIdLimited &&
         Equal(color_metadata.mastering_metadata, kEmptyMasteringMetadata);
}

jint SbMediaPrimaryIdToColorStandard(SbMediaPrimaryId primary_id) {
  switch (primary_id) {
    case kSbMediaPrimaryIdBt709:
      return COLOR_STANDARD_BT709;
    case kSbMediaPrimaryIdBt2020:
      return COLOR_STANDARD_BT2020;
    default:
      return COLOR_VALUE_UNKNOWN;
  }
}

jint SbMediaTransferIdToColorTransfer(SbMediaTransferId transfer_id) {
  switch (transfer_id) {
    case kSbMediaTransferIdBt709:
      return COLOR_TRANSFER_SDR_VIDEO;
    case kSbMediaTransferIdSmpteSt2084:
      return COLOR_TRANSFER_ST2084;
    case kSbMediaTransferIdAribStdB67:
      return COLOR_TRANSFER_HLG;
    default:
      return COLOR_VALUE_UNKNOWN;
  }
}

jint SbMediaRangeIdToColorRange(SbMediaRangeId range_id) {
  switch (range_id) {
    case kSbMediaRangeIdLimited:
      return COLOR_RANGE_LIMITED;
    case kSbMediaRangeIdFull:
      return COLOR_RANGE_FULL;
    case kSbMediaRangeIdUnspecified:
      return COLOR_RANGE_UNSPECIFIED;
    default:
      return COLOR_VALUE_UNKNOWN;
  }
}

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
  if (error_occurred_.load()) {
    SB_LOG(ERROR) << "Tried to seek after an error occurred.";
    return;
  }

  Java_ExoPlayerBridge_seek(AttachCurrentThread(), j_exoplayer_bridge_,
                            seek_to_timestamp);

  std::lock_guard lock(mutex_);
  ended_ = false;
  seek_time_ = seek_to_timestamp;
  playback_ended_ = false;
  audio_eos_written_ = false;
  video_eos_written_ = false;
  seeking_ = true;
  underflow_ = false;
}

void ExoPlayerBridge::WriteSamples(const InputBuffers& input_buffers) {
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
  int offset = type == kSbMediaTypeAudio &&
                       audio_stream_info_.codec == kSbMediaAudioCodecAac
                   ? kADTSHeaderSize
                   : kNoOffset;

  JNIEnv* env = AttachCurrentThread();
  const size_t number_of_samples = input_buffers.size();

  std::vector<jint> sizes(number_of_samples);
  std::vector<jlong> timestamps(number_of_samples);
  std::vector<jboolean> key_frames(number_of_samples);
  size_t total_size = 0;

  for (size_t i = 0; i < number_of_samples; ++i) {
    auto& input_buffer = input_buffers[i];
    total_size += input_buffer->size() - offset;
    const int data_size = input_buffer->size() - offset;
    sizes[i] = data_size;
    timestamps[i] = static_cast<jlong>(input_buffer->timestamp());
    key_frames[i] = type == kSbMediaTypeVideo
                        ? input_buffer->video_sample_info().is_key_frame
                        : true;
  }

  ScopedJavaLocalRef<jintArray> j_sizes(env,
                                        env->NewIntArray(number_of_samples));
  env->SetIntArrayRegion(j_sizes.obj(), 0, number_of_samples, sizes.data());

  ScopedJavaLocalRef<jlongArray> j_timestamps(
      env, env->NewLongArray(number_of_samples));
  env->SetLongArrayRegion(j_timestamps.obj(), 0, number_of_samples,
                          timestamps.data());

  ScopedJavaLocalRef<jbooleanArray> j_key_frames(
      env, env->NewBooleanArray(number_of_samples));
  env->SetBooleanArrayRegion(j_key_frames.obj(), 0, number_of_samples,
                             key_frames.data());

  auto j_media_source =
      type == kSbMediaTypeAudio ? j_audio_media_source_ : j_video_media_source_;
  ScopedJavaLocalRef<jobject> j_combined_samples =
      Java_ExoPlayerMediaSource_getSampleBuffer(env, j_media_source,
                                                total_size);
  SB_CHECK(j_combined_samples);

  uint8_t* j_combined_samples_ptr = static_cast<uint8_t*>(
      env->GetDirectBufferAddress(j_combined_samples.obj()));
  SB_CHECK(j_combined_samples_ptr);

  uint8_t* current_write_ptr = j_combined_samples_ptr;
  for (size_t i = 0; i < number_of_samples; ++i) {
    auto& input_buffer = input_buffers[i];
    const uint8_t* data_start = input_buffer->data() + offset;
    int sample_size = sizes[i];

    memcpy(current_write_ptr, data_start, sample_size);
    current_write_ptr += sample_size;
  }

  Java_ExoPlayerMediaSource_writeSamples(
      env, j_media_source, j_combined_samples, j_sizes, j_timestamps,
      j_key_frames, input_buffers.size());
}

void ExoPlayerBridge::WriteEndOfStream(SbMediaType stream_type) {
  auto media_source = stream_type == kSbMediaTypeAudio ? j_audio_media_source_
                                                       : j_video_media_source_;
  Java_ExoPlayerMediaSource_writeEndOfStream(AttachCurrentThread(),
                                             media_source);

  std::lock_guard lock(mutex_);
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

  std::lock_guard lock(mutex_);
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
    std::lock_guard lock(mutex_);
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
  std::lock_guard lock(mutex_);
  underflow_ = true;
}

void ExoPlayerBridge::SetPlayingStatus(JNIEnv* env, jboolean is_playing) {
  std::lock_guard lock(mutex_);
  if (error_occurred_.load()) {
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

void ExoPlayerBridge::OnSurfaceDestroyed() {
  std::lock_guard lock(mutex_);
  if (is_playing_ && j_video_media_source_ &&
      !IsEndOfStreamWritten(kSbMediaTypeVideo)) {
    SB_LOG(INFO) << "Error: Video surface was destroyed during playback.";
    error_cb_(kSbPlayerErrorDecode,
              "Video surface was destroyed during playback.");
    error_occurred_.store(true);
  }
}

bool ExoPlayerBridge::EnsurePlayerIsInitialized() {
  bool completed_init;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    completed_init = initialized_cv_.wait_for(
        lock, std::chrono::microseconds(kWaitForInitializedTimeoutUs),
        [this] { return initialized_; });
  }

  if (!completed_init) {
    std::string error_message = starboard::FormatString(
        "ExoPlayer initialization exceeded the %d timeout threshold.",
        kWaitForInitializedTimeoutUs);
    SB_LOG(ERROR) << error_message;
    error_cb_(kSbPlayerErrorDecode, error_message.c_str());
    error_occurred_.store(true);
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
    error_occurred_.store(true);
    return;
  }

  j_exoplayer_manager_.Reset(j_exoplayer_manager);

  ScopedJavaLocalRef<jobject> j_exoplayer_bridge(
      Java_ExoPlayerManager_createExoPlayerBridge(
          env, j_exoplayer_manager_, reinterpret_cast<jlong>(this),
          /*prefer tunnel mode*/ false));
  if (!j_exoplayer_bridge) {
    SB_LOG(ERROR) << "Failed to create ExoPlayerBridge.";
    error_occurred_.store(true);
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
      error_occurred_.store(true);
      SB_LOG(ERROR) << "Could not create ExoPlayer audio media source.";
      return;
    }

    ScopedJavaLocalRef<jobject> j_audio_media_source(
        Java_ExoPlayerBridge_createAudioMediaSource(
            env, j_exoplayer_bridge_, exoplayer_codec.value(), j_audio_mime,
            configuration_data, samplerate, channels, bits_per_sample));
    if (!j_audio_media_source) {
      SB_LOG(ERROR) << "Could not create ExoPlayer audio media source.";
      error_occurred_.store(true);
      return;
    }
    j_audio_media_source_.Reset(j_audio_media_source);
  }

  if (video_stream_info_.codec != kSbMediaVideoCodecNone) {
    ScopedJavaGlobalRef<jobject> j_output_surface(env, AcquireVideoSurface());
    if (!j_output_surface) {
      SB_LOG(ERROR) << "Could not acquire video surface for ExoPlayer.";
      error_occurred_.store(true);
      return;
    }
    j_output_surface_.Reset(j_output_surface);

    int width = video_stream_info_.frame_size.width;
    int height = video_stream_info_.frame_size.height;
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

    ScopedJavaLocalRef<jobject> j_color_info(nullptr);
    SbMediaColorMetadata color_metadata = video_stream_info_.color_metadata;
    if (!IsIdentity(color_metadata)) {
      jint color_standard =
          SbMediaPrimaryIdToColorStandard(color_metadata.primaries);
      jint color_transfer =
          SbMediaTransferIdToColorTransfer(color_metadata.transfer);
      jint color_range = SbMediaRangeIdToColorRange(color_metadata.range);

      if (color_standard != COLOR_VALUE_UNKNOWN &&
          color_transfer != COLOR_VALUE_UNKNOWN &&
          color_range != COLOR_VALUE_UNKNOWN) {
        const auto& mastering_metadata = color_metadata.mastering_metadata;
        j_color_info.Reset(Java_ExoPlayerFormatCreator_createColorInfo(
            env, color_range, color_standard, color_transfer,
            mastering_metadata.primary_r_chromaticity_x,
            mastering_metadata.primary_r_chromaticity_y,
            mastering_metadata.primary_g_chromaticity_x,
            mastering_metadata.primary_g_chromaticity_y,
            mastering_metadata.primary_b_chromaticity_x,
            mastering_metadata.primary_b_chromaticity_y,
            mastering_metadata.white_point_chromaticity_x,
            mastering_metadata.white_point_chromaticity_y,
            mastering_metadata.luminance_max, mastering_metadata.luminance_min,
            color_metadata.max_cll, color_metadata.max_fall));
      }
    }

    ScopedJavaLocalRef<jobject> j_video_media_source(
        Java_ExoPlayerBridge_createVideoMediaSource(
            env, j_exoplayer_bridge_, j_mime, j_output_surface_, width, height,
            framerate, bitrate, j_color_info));
    if (!j_video_media_source) {
      SB_LOG(ERROR) << "Could not create ExoPlayer video MediaSource.";
      error_occurred_.store(true);
      return;
    }
    j_video_media_source_.Reset(j_video_media_source);
  }

  int max_samples_per_write = 256;
  j_sizes_.Reset(env, env->NewIntArray(max_samples_per_write));
  j_timestamps_.Reset(env, env->NewLongArray(max_samples_per_write));
  j_key_frames_.Reset(env, env->NewBooleanArray(max_samples_per_write));

  Java_ExoPlayerBridge_preparePlayer(env, j_exoplayer_bridge_);
}

}  // namespace starboard
