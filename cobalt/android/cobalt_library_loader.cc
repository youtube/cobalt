// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "base/android/jni_android.h"
#include "cobalt/cobalt_main_delegate.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"

#include "starboard/android/shared/jni_state.h"

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  // Also pass VM handle to Starboard JNI env
  starboard::android::shared::JNIState::SetVM(vm);
  if (!content::android::OnJNIOnLoadInit()) {
    return -1;
  }
  content::SetContentMainDelegate(new cobalt::CobaltMainDelegate());
  return JNI_VERSION_1_4;
}
