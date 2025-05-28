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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_OUTPUT_MANAGER_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_OUTPUT_MANAGER_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/singleton.h"
#include "starboard/media.h"

namespace starboard {
namespace android {
namespace shared {

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

class AudioOutputManager {
 public:
  // Returns the singleton.
  static AudioOutputManager* GetInstance();

  ScopedJavaLocalRef<jobject> CreateAudioTrackBridge(
      JNIEnv* env,
      int sample_type,
      int sample_rate,
      int channel_count,
      int preferred_buffer_size_in_bytes,
      int tunnel_mode_audio_session_id,
      jboolean is_web_audio);

  void DestroyAudioTrackBridge(JNIEnv* env, ScopedJavaLocalRef<jobject>& obj);

  bool GetOutputDeviceInfo(JNIEnv* env,
                           jint index,
                           ScopedJavaLocalRef<jobject>& obj);

  int GetMinBufferSize(JNIEnv* env,
                       jint sample_type,
                       jint sample_rate,
                       jint channel_count);

  int GetMinBufferSizeInFrames(JNIEnv* env,
                               SbMediaAudioSampleType sample_type,
                               int channels,
                               int sampling_frequency_hz);

  bool GetAndResetHasAudioDeviceChanged(JNIEnv* env);

 private:
  // The constructor runs exactly once because of
  // base::DefaultSingletonTraits<AudioOutputManager>.
  AudioOutputManager();
  ~AudioOutputManager() = default;

  // Prevent copy construction and assignment
  AudioOutputManager(const AudioOutputManager&) = delete;
  AudioOutputManager& operator=(const AudioOutputManager&) = delete;

  friend struct base::DefaultSingletonTraits<AudioOutputManager>;

  // Java AudioOutputManager instance.
  ScopedJavaGlobalRef<jobject> j_audio_output_manager_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  //  STARBOARD_ANDROID_SHARED_AUDIO_OUTPUT_MANAGER_H_
