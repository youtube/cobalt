// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/media.h"

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;

bool SbMediaGetAudioConfiguration(
    int output_index,
    SbMediaAudioConfiguration* out_configuration) {
  if (output_index != 0 || out_configuration == NULL) {
    return false;
  }

  out_configuration->index = 0;
  out_configuration->connector = kSbMediaAudioConnectorHdmi;
  out_configuration->latency = 0;
  out_configuration->coding_type = kSbMediaAudioCodingTypePcm;

  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> j_audio_output_manager(
      env->CallActivityObjectMethodOrAbort(
          "getAudioOutputManager", "()Lfoo/cobalt/media/AudioOutputManager;"));
  int channels = static_cast<int>(env->CallIntMethodOrAbort(
      j_audio_output_manager.Get(), "getMaxChannels", "()I"));
  if (channels < 2) {
    SB_DLOG(WARNING)
        << "The supported channels from output device is smaller than 2. "
           "Fallback to 2 channels";
    out_configuration->number_of_channels = 2;
  } else {
    out_configuration->number_of_channels = channels;
  }
  return true;
}
