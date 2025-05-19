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
#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/AudioOutputManager_jni.h"

namespace starboard {
namespace android {
namespace shared {

SB_EXPORT_ANDROID AudioOutputManager* AudioOutputManager::GetInstance() {
  return base::Singleton<AudioOutputManager>::get();
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
