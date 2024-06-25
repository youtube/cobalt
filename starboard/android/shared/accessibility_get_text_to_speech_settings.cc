// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/memory.h"

#include "starboard/android/shared/accessibility_extension.h"

namespace starboard {
namespace android {
namespace shared {
namespace accessibility {

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;

bool GetTextToSpeechSettings(SbAccessibilityTextToSpeechSettings* out_setting) {
  if (!out_setting ||
      !starboard::common::MemoryIsZero(
          out_setting, sizeof(SbAccessibilityTextToSpeechSettings))) {
    return false;
  }

  JniEnvExt* env = JniEnvExt::Get();

  out_setting->has_text_to_speech_setting = true;
  ScopedLocalJavaRef<jobject> j_tts_helper(
      env->CallStarboardObjectMethodOrAbort(
          "getTextToSpeechHelper",
          "()Ldev/cobalt/coat/CobaltTextToSpeechHelper;"));
  out_setting->is_text_to_speech_enabled = env->CallBooleanMethodOrAbort(
      j_tts_helper.Get(), "isScreenReaderEnabled", "()Z");

  return true;
}

}  // namespace accessibility
}  // namespace shared
}  // namespace android
}  // namespace starboard
