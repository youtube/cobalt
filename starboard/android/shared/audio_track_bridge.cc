// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/audio_track_bridge.h"

#include <algorithm>

#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

using ::starboard::shared::starboard::media::GetBytesPerSample;

const jint kNoOffset = 0;

}  // namespace

AudioTrackBridge::AudioTrackBridge(SbMediaAudioCodingType coding_type,
                                   optional<SbMediaAudioSampleType> sample_type,
                                   int channels,
                                   int sampling_frequency_hz,
                                   int preferred_buffer_size_in_bytes,
                                   int tunnel_mode_audio_session_id,
                                   bool is_web_audio) {
  if (coding_type == kSbMediaAudioCodingTypePcm) {
    SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(sample_type.value()));

    // TODO: Support query if platform supports float type for tunnel mode.
    if (tunnel_mode_audio_session_id != -1) {
      SB_DCHECK(sample_type.value() == kSbMediaAudioSampleTypeInt16Deprecated);
    }
  } else {
    SB_DCHECK(coding_type == kSbMediaAudioCodingTypeAc3 ||
              coding_type == kSbMediaAudioCodingTypeDolbyDigitalPlus);
    // TODO: Support passthrough under tunnel mode.
    SB_DCHECK(tunnel_mode_audio_session_id == -1);
    // TODO: |sample_type| is not used in passthrough mode, we should make this
    // explicit.
  }

  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> j_audio_output_manager(
      env->CallStarboardObjectMethodOrAbort(
          "getAudioOutputManager", "()Ldev/cobalt/media/AudioOutputManager;"));
  jobject j_audio_track_bridge = env->CallObjectMethodOrAbort(
      j_audio_output_manager.Get(), "createAudioTrackBridge",
      "(IIIIIZ)Ldev/cobalt/media/AudioTrackBridge;",
      GetAudioFormatSampleType(coding_type, sample_type), sampling_frequency_hz,
      channels, preferred_buffer_size_in_bytes, tunnel_mode_audio_session_id,
      is_web_audio);
  if (!j_audio_track_bridge) {
    // One of the cases that this may hit is when output happened to be switched
    // to a device that doesn't support tunnel mode.
    // TODO: Find a way to exclude the device from tunnel mode playback, to
    //       avoid infinite loop in creating the audio sink on a device
    //       claims to support tunnel mode but fails to create the audio sink.
    // TODO: Currently this will be reported as a general decode error,
    //       investigate if this can be reported as a capability changed error.
    SB_LOG(WARNING) << "Failed to create |j_audio_track_bridge|.";
    return;
  }
  j_audio_track_bridge_ = env->ConvertLocalRefToGlobalRef(j_audio_track_bridge);
  if (coding_type != kSbMediaAudioCodingTypePcm) {
    // This must be passthrough.
    SB_DCHECK(!sample_type);
    max_samples_per_write_ = kMaxFramesPerRequest;
    j_audio_data_ = env->NewByteArray(max_samples_per_write_);
  } else if (sample_type == kSbMediaAudioSampleTypeFloat32) {
    max_samples_per_write_ = channels * kMaxFramesPerRequest;
    j_audio_data_ = env->NewFloatArray(channels * kMaxFramesPerRequest);
  } else if (sample_type == kSbMediaAudioSampleTypeInt16Deprecated) {
    max_samples_per_write_ = channels * kMaxFramesPerRequest;
    j_audio_data_ =
        env->NewByteArray(channels * GetBytesPerSample(sample_type.value()) *
                          kMaxFramesPerRequest);
  } else {
    SB_NOTREACHED();
  }
  SB_DCHECK(j_audio_data_) << "Failed to allocate |j_audio_data_|";

  j_audio_data_ = env->ConvertLocalRefToGlobalRef(j_audio_data_);
}

AudioTrackBridge::~AudioTrackBridge() {
  JniEnvExt* env = JniEnvExt::Get();
  if (j_audio_track_bridge_) {
    ScopedLocalJavaRef<jobject> j_audio_output_manager(
        env->CallStarboardObjectMethodOrAbort(
            "getAudioOutputManager",
            "()Ldev/cobalt/media/AudioOutputManager;"));
    env->CallVoidMethodOrAbort(
        j_audio_output_manager.Get(), "destroyAudioTrackBridge",
        "(Ldev/cobalt/media/AudioTrackBridge;)V", j_audio_track_bridge_);
    env->DeleteGlobalRef(j_audio_track_bridge_);
    j_audio_track_bridge_ = nullptr;
  }

  if (j_audio_data_) {
    env->DeleteGlobalRef(j_audio_data_);
    j_audio_data_ = nullptr;
  }
}

// static
int AudioTrackBridge::GetMinBufferSizeInFrames(
    SbMediaAudioSampleType sample_type,
    int channels,
    int sampling_frequency_hz,
    JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);

  ScopedLocalJavaRef<jobject> j_audio_output_manager(
      env->CallStarboardObjectMethodOrAbort(
          "getAudioOutputManager", "()Ldev/cobalt/media/AudioOutputManager;"));
  int audio_track_min_buffer_size = static_cast<int>(env->CallIntMethodOrAbort(
      j_audio_output_manager.Get(), "getMinBufferSize", "(III)I",
      GetAudioFormatSampleType(kSbMediaAudioCodingTypePcm, sample_type),
      sampling_frequency_hz, channels));
  return audio_track_min_buffer_size / channels /
         GetBytesPerSample(sample_type);
}

void AudioTrackBridge::Play(JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  env->CallVoidMethodOrAbort(j_audio_track_bridge_, "play", "()V");
  SB_LOG(INFO) << "AudioTrackBridge playing.";
}

void AudioTrackBridge::Pause(JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  env->CallVoidMethodOrAbort(j_audio_track_bridge_, "pause", "()V");
  SB_LOG(INFO) << "AudioTrackBridge paused.";
}

void AudioTrackBridge::Stop(JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  env->CallVoidMethodOrAbort(j_audio_track_bridge_, "stop", "()V");
  SB_LOG(INFO) << "AudioTrackBridge stopped.";
}

void AudioTrackBridge::PauseAndFlush(JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  // For an immediate stop, use pause(), followed by flush() to discard audio
  // data that hasn't been played back yet.
  env->CallVoidMethodOrAbort(j_audio_track_bridge_, "pause", "()V");
  // Flushes the audio data currently queued for playback. Any data that has
  // been written but not yet presented will be discarded.
  env->CallVoidMethodOrAbort(j_audio_track_bridge_, "flush", "()V");
}

int AudioTrackBridge::WriteSample(const float* samples,
                                  int num_of_samples,
                                  JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());
  SB_DCHECK(num_of_samples <= max_samples_per_write_);

  num_of_samples = std::min(num_of_samples, max_samples_per_write_);
  env->SetFloatArrayRegion(static_cast<jfloatArray>(j_audio_data_), kNoOffset,
                           num_of_samples, samples);
  return env->CallIntMethodOrAbort(j_audio_track_bridge_, "write", "([FI)I",
                                   j_audio_data_, num_of_samples);
}

int AudioTrackBridge::WriteSample(const uint16_t* samples,
                                  int num_of_samples,
                                  SbTime sync_time,
                                  JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());
  SB_DCHECK(num_of_samples <= max_samples_per_write_);

  num_of_samples = std::min(num_of_samples, max_samples_per_write_);
  env->SetByteArrayRegion(static_cast<jbyteArray>(j_audio_data_), kNoOffset,
                          num_of_samples * sizeof(uint16_t),
                          reinterpret_cast<const jbyte*>(samples));

  int bytes_written = env->CallIntMethodOrAbort(
      j_audio_track_bridge_, "write", "([BIJ)I", j_audio_data_,
      num_of_samples * sizeof(uint16_t), sync_time);
  if (bytes_written < 0) {
    // Error code returned as negative value, like AudioTrack.ERROR_DEAD_OBJECT.
    return bytes_written;
  }
  SB_DCHECK(bytes_written % sizeof(uint16_t) == 0);
  return bytes_written / sizeof(uint16_t);
}

int AudioTrackBridge::WriteSample(const uint8_t* samples,
                                  int num_of_samples,
                                  SbTime sync_time,
                                  JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());
  SB_DCHECK(num_of_samples <= max_samples_per_write_);

  num_of_samples = std::min(num_of_samples, max_samples_per_write_);

  env->SetByteArrayRegion(static_cast<jbyteArray>(j_audio_data_), kNoOffset,
                          num_of_samples,
                          reinterpret_cast<const jbyte*>(samples));

  int bytes_written =
      env->CallIntMethodOrAbort(j_audio_track_bridge_, "write", "([BIJ)I",
                                j_audio_data_, num_of_samples, sync_time);
  if (bytes_written < 0) {
    // Error code returned as negative value, like AudioTrack.ERROR_DEAD_OBJECT.
    return bytes_written;
  }
  return bytes_written;
}

void AudioTrackBridge::SetVolume(double volume,
                                 JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  jint status = env->CallIntMethodOrAbort(j_audio_track_bridge_, "setVolume",
                                          "(F)I", static_cast<float>(volume));
  if (status != 0) {
    SB_LOG(ERROR) << "Failed to set volume to " << volume;
  }
}

int64_t AudioTrackBridge::GetAudioTimestamp(
    SbTime* updated_at,
    JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  ScopedLocalJavaRef<jobject> j_audio_timestamp(
      env->CallObjectMethodOrAbort(j_audio_track_bridge_, "getAudioTimestamp",
                                   "()Landroid/media/AudioTimestamp;"));
  if (updated_at) {
    *updated_at =
        env->GetLongFieldOrAbort(j_audio_timestamp.Get(), "nanoTime", "J") /
        kSbTimeNanosecondsPerMicrosecond;
  }
  return env->GetLongFieldOrAbort(j_audio_timestamp.Get(), "framePosition",
                                  "J");
}

bool AudioTrackBridge::GetAndResetHasAudioDeviceChanged(
    JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  ScopedLocalJavaRef<jobject> j_audio_output_manager(
      env->CallStarboardObjectMethodOrAbort(
          "getAudioOutputManager", "()Ldev/cobalt/media/AudioOutputManager;"));

  return env->CallBooleanMethodOrAbort(
      j_audio_output_manager.Get(), "getAndResetHasAudioDeviceChanged", "()Z");
}

int AudioTrackBridge::GetUnderrunCount(JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  return env->CallIntMethodOrAbort(j_audio_track_bridge_, "getUnderrunCount",
                                   "()I");
}

int AudioTrackBridge::GetStartThresholdInFrames(
    JniEnvExt* env /*= JniEnvExt::Get()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  return env->CallIntMethodOrAbort(j_audio_track_bridge_,
                                   "getStartThresholdInFrames", "()I");
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
