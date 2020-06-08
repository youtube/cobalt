// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/window.h"

#include "starboard/android/shared/jni_env_ext.h"

using starboard::android::shared::JniEnvExt;

#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
void SbWindowSetOnScreenKeyboardKeepFocus(SbWindow window, bool keep_focus) {
  JniEnvExt* env = JniEnvExt::Get();
  jobject j_keyboard_editor = env->CallStarboardObjectMethodOrAbort(
      "getKeyboardEditor", "()Ldev/cobalt/coat/KeyboardEditor;");
  env->CallVoidMethodOrAbort(j_keyboard_editor, "updateKeepFocus", "(Z)V",
                             keep_focus);
  return;
}
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)
