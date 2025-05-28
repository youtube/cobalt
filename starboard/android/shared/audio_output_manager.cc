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

#include "starboard/android/shared/audio_output_manager.h"

#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/AudioOutputManager_jni.h"

namespace starboard {
namespace android {
namespace shared {

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AttachCurrentThread;

AudioOutputManager::AudioOutputManager() {
  JNIEnv* env = AttachCurrentThread();
  SB_DCHECK(env);
  j_audio_output_manager_ =
      StarboardBridge::GetInstance()->GetAudioOutputManager(env);
}

SB_EXPORT_ANDROID AudioOutputManager* AudioOutputManager::GetInstance() {
  return base::Singleton<AudioOutputManager>::get();
}

ScopedJavaLocalRef<jobject> AudioOutputManager::CreateAudioTrackBridge(
    JNIEnv* env,
    int sample_type,
    int sample_rate,
    int channel_count,
    int preferred_buffer_size_in_bytes,
    int tunnel_mode_audio_session_id,
    jboolean is_web_audio) {
  SB_DCHECK(env);
  return Java_AudioOutputManager_createAudioTrackBridge(
      env, j_audio_output_manager_, sample_type, sample_rate, channel_count,
      preferred_buffer_size_in_bytes, tunnel_mode_audio_session_id,
      is_web_audio);
}

void AudioOutputManager::DestroyAudioTrackBridge(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject>& obj) {
  SB_DCHECK(env);
  return Java_AudioOutputManager_destroyAudioTrackBridge(
      env, j_audio_output_manager_, obj);
}

bool AudioOutputManager::GetOutputDeviceInfo(JNIEnv* env,
                                             jint index,
                                             ScopedJavaLocalRef<jobject>& obj) {
  SB_DCHECK(env);
  jboolean output_device_info_java =
      Java_AudioOutputManager_getOutputDeviceInfo(env, j_audio_output_manager_,
                                                  index, obj);

  return output_device_info_java == JNI_TRUE;
}

int AudioOutputManager::GetMinBufferSize(JNIEnv* env,
                                         jint sample_type,
                                         jint sample_rate,
                                         jint channel_count) {
  SB_DCHECK(env);
  return Java_AudioOutputManager_getMinBufferSize(
      env, j_audio_output_manager_, sample_type, sample_rate, channel_count);
}

int AudioOutputManager::GetMinBufferSizeInFrames(
    JNIEnv* env,
    SbMediaAudioSampleType sample_type,
    int channels,
    int sampling_frequency_hz) {
  int audio_track_min_buffer_size = GetMinBufferSize(
      env, GetAudioFormatSampleType(kSbMediaAudioCodingTypePcm, sample_type),
      sampling_frequency_hz, channels);
  return audio_track_min_buffer_size / channels /
         ::starboard::shared::starboard::media::GetBytesPerSample(sample_type);
}

bool AudioOutputManager::GetAndResetHasAudioDeviceChanged(JNIEnv* env) {
  SB_DCHECK(env);
  return Java_AudioOutputManager_getAndResetHasAudioDeviceChanged(
             env, j_audio_output_manager_) == JNI_TRUE;
}

extern "C" SB_EXPORT_PLATFORM void JNI_AudioOutputManager_OnAudioDeviceChanged(
    JNIEnv* env) {
  // Audio output device change could change passthrough decoder capabilities,
  // so we have to reload codec capabilities.
  MediaCapabilitiesCache::GetInstance()->ClearCache();
  ::starboard::shared::starboard::media::MimeSupportabilityCache::GetInstance()
      ->ClearCachedMimeSupportabilities();
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
