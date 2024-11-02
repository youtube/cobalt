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

#include "base/android/base_jni_onload.h"
#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "cobalt/cobalt_main_delegate.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"

#include "starboard/android/shared/jni_vars.h"

#include <android/log.h>

bool NativeInit(base::android::LibraryProcessType) {
  __android_log_print(ANDROID_LOG_ERROR, "yolo", "%s", "Native init hook");
  // Nothing to be done here
  return true;
}

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  __android_log_print(ANDROID_LOG_ERROR, "yolo", "%s",
                      "JNI_OnLoad from Cobalt!!");
  base::android::InitVM(vm);

  // Pass stuff to starboard
  starboard::android::shared::JNIState::SetVM(vm);

  base::android::SetNativeInitializationHook(NativeInit);
  if (!content::android::OnJNIOnLoadInit()) {
    __android_log_print(ANDROID_LOG_ERROR, "yolo", "%s",
                        "content::android::OnJNIOnLoadInit failed !!!");
    return -1;
  }

  __android_log_print(ANDROID_LOG_ERROR, "yolo", "%s",
                      "Setting ContentMainDelegate to Cobalt");
  content::SetContentMainDelegate(new cobalt::CobaltMainDelegate());
  __android_log_print(ANDROID_LOG_ERROR, "yolo", "%s", "DONE with YOLO JNI");
  return JNI_VERSION_1_6;
}
