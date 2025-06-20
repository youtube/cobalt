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
#include <mutex>

<<<<<<< HEAD
#include "starboard/android/shared/jni_utils.h"
=======
#include "starboard/android/shared/audio_output_manager.h"
>>>>>>> ab136fd36d4 (Migrate AudioTrackBridge to jni_generator (#6010))
#include "starboard/android/shared/media_common.h"
#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/media_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/AudioTrackBridge_jni.h"

namespace starboard::android::shared {

namespace {

using ::starboard::shared::starboard::media::GetBytesPerSample;

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

void AudioTrackBridge::Play(JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

<<<<<<< HEAD
  env->CallVoidMethodOrAbort(j_audio_track_bridge_, "play", "()V");
=======
  Java_AudioTrackBridge_play(env, j_audio_track_bridge_);
>>>>>>> ab136fd36d4 (Migrate AudioTrackBridge to jni_generator (#6010))
  SB_LOG(INFO) << "AudioTrackBridge playing.";
}

void AudioTrackBridge::Pause(JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

<<<<<<< HEAD
  env->CallVoidMethodOrAbort(j_audio_track_bridge_, "pause", "()V");
=======
  Java_AudioTrackBridge_pause(env, j_audio_track_bridge_);
>>>>>>> ab136fd36d4 (Migrate AudioTrackBridge to jni_generator (#6010))
  SB_LOG(INFO) << "AudioTrackBridge paused.";
}

void AudioTrackBridge::Stop(JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

<<<<<<< HEAD
  env->CallVoidMethodOrAbort(j_audio_track_bridge_, "stop", "()V");
=======
  Java_AudioTrackBridge_stop(env, j_audio_track_bridge_);
>>>>>>> ab136fd36d4 (Migrate AudioTrackBridge to jni_generator (#6010))
  SB_LOG(INFO) << "AudioTrackBridge stopped.";
}

void AudioTrackBridge::PauseAndFlush(JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

  // For an immediate stop, use pause(), followed by flush() to discard audio
  // data that hasn't been played back yet.
<<<<<<< HEAD
  env->CallVoidMethodOrAbort(j_audio_track_bridge_, "pause", "()V");
  // Flushes the audio data currently queued for playback. Any data that has
  // been written but not yet presented will be discarded.
  env->CallVoidMethodOrAbort(j_audio_track_bridge_, "flush", "()V");
=======
  Java_AudioTrackBridge_pause(env, j_audio_track_bridge_);
  // Flushes the audio data currently queued for playback. Any data that has
  // been written but not yet presented will be discarded.
  Java_AudioTrackBridge_flush(env, j_audio_track_bridge_);
>>>>>>> ab136fd36d4 (Migrate AudioTrackBridge to jni_generator (#6010))
}

int AudioTrackBridge::WriteSample(const float* samples,
                                  int num_of_samples,
                                  JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());
  SB_DCHECK(num_of_samples <= max_samples_per_write_);

  num_of_samples = std::min(num_of_samples, max_samples_per_write_);
<<<<<<< HEAD
  env->SetFloatArrayRegion(static_cast<jfloatArray>(j_audio_data_), kNoOffset,
                           num_of_samples, samples);
  return env->CallIntMethodOrAbort(j_audio_track_bridge_, "write", "([FI)I",
                                   j_audio_data_, num_of_samples);
=======

  SB_DCHECK(env->IsInstanceOf(j_audio_data_.obj(), env->FindClass("[F")));
  env->SetFloatArrayRegion(static_cast<jfloatArray>(j_audio_data_.obj()),
                           kNoOffset, num_of_samples, samples);

  ScopedJavaLocalRef<jfloatArray> audio_data_local_ref(
      env, static_cast<jfloatArray>(env->NewLocalRef(j_audio_data_.obj())));

  return Java_AudioTrackBridge_write(env, j_audio_track_bridge_,
                                     // JavaParamRef<jfloatArray>'s raw pointer
                                     // constructor expects a local reference.
                                     audio_data_local_ref, num_of_samples);
>>>>>>> ab136fd36d4 (Migrate AudioTrackBridge to jni_generator (#6010))
}

int AudioTrackBridge::WriteSample(const uint16_t* samples,
                                  int num_of_samples,
                                  int64_t sync_time,
                                  JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());
  SB_DCHECK(num_of_samples <= max_samples_per_write_);

  num_of_samples = std::min(num_of_samples, max_samples_per_write_);
<<<<<<< HEAD
  env->SetByteArrayRegion(static_cast<jbyteArray>(j_audio_data_), kNoOffset,
                          num_of_samples * sizeof(uint16_t),
                          reinterpret_cast<const jbyte*>(samples));

  int bytes_written = env->CallIntMethodOrAbort(
      j_audio_track_bridge_, "write", "([BIJ)I", j_audio_data_,
      num_of_samples * sizeof(uint16_t), sync_time);
=======

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
>>>>>>> ab136fd36d4 (Migrate AudioTrackBridge to jni_generator (#6010))
  if (bytes_written < 0) {
    // Error code returned as negative value, like AudioTrack.ERROR_DEAD_OBJECT.
    return bytes_written;
  }
  SB_DCHECK(bytes_written % sizeof(uint16_t) == 0);

  return bytes_written / sizeof(uint16_t);
}

int AudioTrackBridge::WriteSample(const uint8_t* samples,
                                  int num_of_samples,
                                  int64_t sync_time,
                                  JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());
  SB_DCHECK(num_of_samples <= max_samples_per_write_);

  num_of_samples = std::min(num_of_samples, max_samples_per_write_);

<<<<<<< HEAD
  env->SetByteArrayRegion(static_cast<jbyteArray>(j_audio_data_), kNoOffset,
                          num_of_samples,
                          reinterpret_cast<const jbyte*>(samples));

  int bytes_written =
      env->CallIntMethodOrAbort(j_audio_track_bridge_, "write", "([BIJ)I",
                                j_audio_data_, num_of_samples, sync_time);
=======
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

>>>>>>> ab136fd36d4 (Migrate AudioTrackBridge to jni_generator (#6010))
  if (bytes_written < 0) {
    // Error code returned as negative value, like AudioTrack.ERROR_DEAD_OBJECT.
    return bytes_written;
  }
  return bytes_written;
}

void AudioTrackBridge::SetVolume(double volume,
                                 JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

<<<<<<< HEAD
  jint status = env->CallIntMethodOrAbort(j_audio_track_bridge_, "setVolume",
                                          "(F)I", static_cast<float>(volume));
=======
  jint status = Java_AudioTrackBridge_setVolume(env, j_audio_track_bridge_,
                                                static_cast<float>(volume));
>>>>>>> ab136fd36d4 (Migrate AudioTrackBridge to jni_generator (#6010))
  if (status != 0) {
    SB_LOG(ERROR) << "Failed to set volume to " << volume;
  }
}

int64_t AudioTrackBridge::GetAudioTimestamp(
    int64_t* updated_at,
    JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

<<<<<<< HEAD
  ScopedLocalJavaRef<jobject> j_audio_timestamp(
      env->CallObjectMethodOrAbort(j_audio_track_bridge_, "getAudioTimestamp",
                                   "()Landroid/media/AudioTimestamp;"));
=======
  // Cache the class and field IDs to avoid the overhead induced by the frequent
  // lookup.
  struct AudioTimestampJniCache {
    jclass timestamp_class = nullptr;
    jfieldID nano_time_field = nullptr;
    jfieldID frame_position_field = nullptr;
  };

  static std::once_flag once_flag;
  static AudioTimestampJniCache cache;

  std::call_once(once_flag, [env]() {
    jclass local_class = env->FindClass("android/media/AudioTimestamp");
    cache.timestamp_class = static_cast<jclass>(env->NewGlobalRef(local_class));
    env->DeleteLocalRef(local_class);
    SB_DCHECK(cache.timestamp_class);

    cache.nano_time_field =
        env->GetFieldID(cache.timestamp_class, "nanoTime", "J");
    SB_DCHECK(cache.nano_time_field);

    cache.frame_position_field =
        env->GetFieldID(cache.timestamp_class, "framePosition", "J");
    SB_DCHECK(cache.frame_position_field);
  });

  ScopedJavaLocalRef<jobject> j_audio_timestamp =
      Java_AudioTrackBridge_getAudioTimestamp(env, j_audio_track_bridge_);
  SB_DCHECK(!j_audio_timestamp.is_null());

>>>>>>> ab136fd36d4 (Migrate AudioTrackBridge to jni_generator (#6010))
  if (updated_at) {
    *updated_at =
        env->GetLongField(j_audio_timestamp.obj(), cache.nano_time_field) /
        1000;
  }
  return env->GetLongField(j_audio_timestamp.obj(), cache.frame_position_field);
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

int AudioTrackBridge::GetUnderrunCount(
    JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

<<<<<<< HEAD
  return env->CallIntMethodOrAbort(j_audio_track_bridge_, "getUnderrunCount",
                                   "()I");
=======
  return Java_AudioTrackBridge_getUnderrunCount(env, j_audio_track_bridge_);
>>>>>>> ab136fd36d4 (Migrate AudioTrackBridge to jni_generator (#6010))
}

int AudioTrackBridge::GetStartThresholdInFrames(
    JNIEnv* env /*= AttachCurrentThread()*/) {
  SB_DCHECK(env);
  SB_DCHECK(is_valid());

<<<<<<< HEAD
  return env->CallIntMethodOrAbort(j_audio_track_bridge_,
                                   "getStartThresholdInFrames", "()I");
=======
  return Java_AudioTrackBridge_getStartThresholdInFrames(env,
                                                         j_audio_track_bridge_);
>>>>>>> ab136fd36d4 (Migrate AudioTrackBridge to jni_generator (#6010))
}

}  // namespace starboard::android::shared
