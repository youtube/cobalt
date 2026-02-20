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

#include "starboard/android/shared/audio_output_manager.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/media_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/AudioTrackBridge_jni.h"
#include "starboard/common/check_op.h"

namespace starboard {

namespace {

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using ::base::android::AttachCurrentThread;
using ::base::android::JavaParamRef;
using ::base::android::ScopedJavaGlobalRef;
using ::base::android::ScopedJavaLocalRef;

const jint kNoOffset = 0;

}  // namespace

AudioTrackBridge::AudioTrackBridge(
    SbMediaAudioCodingType coding_type,
    std::optional<SbMediaAudioSampleType> sample_type,
    int channels,
    int sampling_frequency_hz,
    int preferred_buffer_size_in_bytes,
    int tunnel_mode_audio_session_id,
    bool is_web_audio) {
  if (coding_type == kSbMediaAudioCodingTypePcm) {
    SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(sample_type.value()));

    // TODO: Support query if platform supports float type for tunnel mode.
    if (tunnel_mode_audio_session_id != -1) {
      SB_DCHECK_EQ(sample_type.value(), kSbMediaAudioSampleTypeInt16Deprecated);
    }
  } else {
    SB_DCHECK(coding_type == kSbMediaAudioCodingTypeAc3 ||
              coding_type == kSbMediaAudioCodingTypeDolbyDigitalPlus);
    // TODO: Support passthrough under tunnel mode.
    SB_DCHECK_EQ(tunnel_mode_audio_session_id, -1);
    // TODO: |sample_type| is not used in passthrough mode, we should make this
    // explicit.
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_audio_track_bridge =
      AudioOutputManager::GetInstance()->CreateAudioTrackBridge(
          env, GetAudioFormatSampleType(coding_type, sample_type),
          sampling_frequency_hz, channels, preferred_buffer_size_in_bytes,
          tunnel_mode_audio_session_id, is_web_audio);

  if (j_audio_track_bridge.is_null()) {
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

  j_audio_track_bridge_.Reset(j_audio_track_bridge);

  if (coding_type != kSbMediaAudioCodingTypePcm) {
    // This must be passthrough.
    SB_DCHECK(!sample_type);
    max_samples_per_write_ = kMaxFramesPerRequest;
    j_audio_data_.Reset(env, env->NewByteArray(max_samples_per_write_));
  } else if (sample_type == kSbMediaAudioSampleTypeFloat32) {
    max_samples_per_write_ = channels * kMaxFramesPerRequest;
    j_audio_data_.Reset(env,
                        env->NewFloatArray(channels * kMaxFramesPerRequest));
  } else if (sample_type == kSbMediaAudioSampleTypeInt16Deprecated) {
    max_samples_per_write_ = channels * kMaxFramesPerRequest;
    j_audio_data_.Reset(
        env,
        env->NewByteArray(channels * GetBytesPerSample(sample_type.value()) *
                          kMaxFramesPerRequest));
  } else {
    SB_NOTREACHED();
  }

  SB_DCHECK(j_audio_data_) << "Failed to allocate |j_audio_data_|";
}

AudioTrackBridge::~AudioTrackBridge() {
  if (!j_audio_track_bridge_.is_null()) {
    // Both the global and local references refer to the exact same Java object
    // in the JVM.
    JNIEnv* env = AttachCurrentThread();
    jobject audio_track_bridge_ref =
        env->NewLocalRef(j_audio_track_bridge_.obj());
    ScopedJavaLocalRef<jobject> audio_track_bridge(env, audio_track_bridge_ref);
    AudioOutputManager::GetInstance()->DestroyAudioTrackBridge(
        env, audio_track_bridge);
  }
}

void AudioTrackBridge::Play(JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  Java_AudioTrackBridge_play(env, j_audio_track_bridge_);
  SB_LOG(INFO) << "AudioTrackBridge playing.";
}

void AudioTrackBridge::Pause(JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  Java_AudioTrackBridge_pause(env, j_audio_track_bridge_);
  SB_LOG(INFO) << "AudioTrackBridge paused.";
}

void AudioTrackBridge::Stop(JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  Java_AudioTrackBridge_stop(env, j_audio_track_bridge_);
  SB_LOG(INFO) << "AudioTrackBridge stopped.";
}

void AudioTrackBridge::PauseAndFlush(JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  // For an immediate stop, use pause(), followed by flush() to discard audio
  // data that hasn't been played back yet.
  Java_AudioTrackBridge_pause(env, j_audio_track_bridge_);
  // Flushes the audio data currently queued for playback. Any data that has
  // been written but not yet presented will be discarded.
  Java_AudioTrackBridge_flush(env, j_audio_track_bridge_);
}

int AudioTrackBridge::WriteSample(const float* samples,
                                  int num_of_samples,
                                  JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());
  SB_DCHECK_LE(num_of_samples, max_samples_per_write_);

  num_of_samples = std::min(num_of_samples, max_samples_per_write_);

  SB_DCHECK(env->IsInstanceOf(j_audio_data_.obj(), env->FindClass("[F")));
  env->SetFloatArrayRegion(static_cast<jfloatArray>(j_audio_data_.obj()),
                           kNoOffset, num_of_samples, samples);

  ScopedJavaLocalRef<jfloatArray> audio_data_local_ref(
      env, static_cast<jfloatArray>(env->NewLocalRef(j_audio_data_.obj())));

  return Java_AudioTrackBridge_write(env, j_audio_track_bridge_,
                                     // JavaParamRef<jfloatArray>'s raw pointer
                                     // constructor expects a local reference.
                                     audio_data_local_ref, num_of_samples);
}

int AudioTrackBridge::WriteSample(const uint16_t* samples,
                                  int num_of_samples,
                                  int64_t sync_time,
                                  JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());
  SB_DCHECK_LE(num_of_samples, max_samples_per_write_);

  num_of_samples = std::min(num_of_samples, max_samples_per_write_);

  SB_DCHECK(env->IsInstanceOf(j_audio_data_.obj(), env->FindClass("[B")));
  env->SetByteArrayRegion(static_cast<jbyteArray>(j_audio_data_.obj()),
                          kNoOffset, num_of_samples * sizeof(uint16_t),
                          reinterpret_cast<const jbyte*>(samples));

  ScopedJavaLocalRef<jbyteArray> audio_data_local_ref(
      env, static_cast<jbyteArray>(env->NewLocalRef(j_audio_data_.obj())));

  int bytes_written = Java_AudioTrackBridge_writeWithPresentationTime(
      env, j_audio_track_bridge_,
      // JavaParamRef<jbyteArray>'s raw pointer constructor expects a local
      // reference.
      audio_data_local_ref, num_of_samples * sizeof(uint16_t), sync_time);
  if (bytes_written < 0) {
    // Error code returned as negative value, like AudioTrack.ERROR_DEAD_OBJECT.
    return bytes_written;
  }
  SB_DCHECK_EQ(bytes_written % sizeof(uint16_t), static_cast<size_t>(0));

  return bytes_written / sizeof(uint16_t);
}

int AudioTrackBridge::WriteSample(const uint8_t* samples,
                                  int num_of_samples,
                                  int64_t sync_time,
                                  JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());
  SB_DCHECK_LE(num_of_samples, max_samples_per_write_);

  num_of_samples = std::min(num_of_samples, max_samples_per_write_);

  SB_DCHECK(env->IsInstanceOf(j_audio_data_.obj(), env->FindClass("[B")));
  env->SetByteArrayRegion(static_cast<jbyteArray>(j_audio_data_.obj()),
                          kNoOffset, num_of_samples,
                          reinterpret_cast<const jbyte*>(samples));

  ScopedJavaLocalRef<jbyteArray> audio_data_local_ref(
      env, static_cast<jbyteArray>(env->NewLocalRef(j_audio_data_.obj())));

  int bytes_written = Java_AudioTrackBridge_writeWithPresentationTime(
      env, j_audio_track_bridge_,
      // JavaParamRef<jbyteArray>'s raw pointer constructor expects a local
      // reference.
      audio_data_local_ref, num_of_samples, sync_time);

  if (bytes_written < 0) {
    // Error code returned as negative value, like AudioTrack.ERROR_DEAD_OBJECT.
    return bytes_written;
  }
  return bytes_written;
}

void AudioTrackBridge::SetPlaybackRate(
    double playback_rate,
    JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());
  // AudioTrack doesn't support playback speed of 0.
  SB_DCHECK(playback_rate > 0.0);

  jint status = Java_AudioTrackBridge_setPlaybackRate(
      env, j_audio_track_bridge_, static_cast<float>(playback_rate));
  if (status != 0) {
    SB_LOG(ERROR) << "Failed to set playback rate to " << playback_rate;
  }
}

void AudioTrackBridge::SetVolume(double volume,
                                 JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  jint status = Java_AudioTrackBridge_setVolume(env, j_audio_track_bridge_,
                                                static_cast<float>(volume));
  if (status != 0) {
    SB_LOG(ERROR) << "Failed to set volume to " << volume;
  }
}

int64_t AudioTrackBridge::GetAudioTimestamp(
    int64_t* updated_at,
    JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  ScopedJavaLocalRef<jobject> j_audio_timestamp =
      Java_AudioTrackBridge_getAudioTimestamp(env, j_audio_track_bridge_);
  SB_DCHECK(!j_audio_timestamp.is_null());

  if (updated_at) {
    *updated_at =
        Java_AudioTimestamp_getNanoTime(env, j_audio_timestamp) / 1000;
  }
  return Java_AudioTimestamp_getFramePosition(env, j_audio_timestamp);
}

bool AudioTrackBridge::GetAndResetHasAudioDeviceChanged(
    JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  return AudioOutputManager::GetInstance()->GetAndResetHasAudioDeviceChanged(
      env);
}

int AudioTrackBridge::GetUnderrunCount(
    JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  return Java_AudioTrackBridge_getUnderrunCount(env, j_audio_track_bridge_);
}

int AudioTrackBridge::GetStartThresholdInFrames(
    JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  return Java_AudioTrackBridge_getStartThresholdInFrames(env,
                                                         j_audio_track_bridge_);
}

}  // namespace starboard
