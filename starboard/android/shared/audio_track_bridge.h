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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_TRACK_BRIDGE_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_TRACK_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/optional.h"
#include "starboard/media.h"
#include "starboard/types.h"

namespace starboard::android::shared {

// TODO(cobalt, b/372559388): Update namespace to jni_zero.
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

// The C++ encapsulation of the Java class AudioTrackBridge.
class AudioTrackBridge {
 public:
  // The maximum number of frames that can be written to android audio track per
  // write request.  It is used to pre-allocate |j_audio_data_|.
  static constexpr int kMaxFramesPerRequest = 65536;
  // The same as Android AudioTrack.ERROR_DEAD_OBJECT.
  static constexpr int kAudioTrackErrorDeadObject = -6;

  AudioTrackBridge(SbMediaAudioCodingType coding_type,
                   optional<SbMediaAudioSampleType> sample_type,
                   int channels,
                   int sampling_frequency_hz,
                   int preferred_buffer_size_in_bytes,
                   int tunnel_mode_audio_session_id,
                   bool is_web_audio);
  ~AudioTrackBridge();

  bool is_valid() const {
    return !j_audio_track_bridge_.is_null() && !j_audio_data_.is_null();
  }

  void Play(JniEnvExt* env = JniEnvExt::Get());
  void Pause(JniEnvExt* env = JniEnvExt::Get());
  void Stop(JniEnvExt* env = JniEnvExt::Get());
  void PauseAndFlush(JniEnvExt* env = JniEnvExt::Get());

  // Returns zero or the positive number of samples written, or a negative error
  // code.
  int WriteSample(const float* samples,
                  int num_of_samples,
                  JniEnvExt* env = JniEnvExt::Get());
  int WriteSample(const uint16_t* samples,
                  int num_of_samples,
                  int64_t sync_time,
                  JniEnvExt* env = JniEnvExt::Get());
  // This is used by passthrough, it treats samples as if they are in bytes.
  // Returns zero or the positive number of samples written, or a negative error
  // code.
  int WriteSample(const uint8_t* buffer,
                  int num_of_samples,
                  int64_t sync_time,
                  JniEnvExt* env = JniEnvExt::Get());

  void SetVolume(double volume, JniEnvExt* env = JniEnvExt::Get());

  // |updated_at| contains the timestamp when the audio timestamp is updated on
  // return.  It can be nullptr.
  int64_t GetAudioTimestamp(int64_t* updated_at,
                            JniEnvExt* env = JniEnvExt::Get());
  bool GetAndResetHasAudioDeviceChanged(JNIEnv* env = AttachCurrentThread());
  int GetUnderrunCount(JniEnvExt* env = JniEnvExt::Get());
  int GetStartThresholdInFrames(JniEnvExt* env = JniEnvExt::Get());

 private:
  int max_samples_per_write_;

  ScopedJavaGlobalRef<jobject> j_audio_track_bridge_;
  // The audio data has to be copied into a Java Array before writing into the
  // audio track. Allocating a large array and saves as a member variable
  // avoids an array being allocated repeatedly.
  ScopedJavaGlobalRef<jobject> j_audio_data_;
};

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_AUDIO_TRACK_BRIDGE_H_
