// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <android/native_window.h>

#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/android/shared/window_internal.h"
#include "starboard/common/log.h"
#include "starboard/window.h"

// TODO: (cobalt b/372559388) Update namespace to jni_zero.
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

bool SbWindowGetSize(SbWindow window, SbWindowSize* size) {
  if (!SbWindowIsValid(window)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid window.";
    return false;
  }

  if (window->native_window == NULL) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Native window has been destroyed.";
    return false;
  }
  size->width = ANativeWindow_getWidth(window->native_window);
  size->height = ANativeWindow_getHeight(window->native_window);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> display_dpi =
      starboard::android::shared::StarboardBridge::GetInstance()
          ->GetDeviceResolution(env);

  jclass sizeClass = env->FindClass("android/util/Size");
  jmethodID getWidthMethod = env->GetMethodID(sizeClass, "getWidth", "()I");
  jmethodID getHeightMethod = env->GetMethodID(sizeClass, "getHeight", "()I");
  int display_width = env->CallIntMethod(display_dpi.obj(), getWidthMethod);
  int display_height = env->CallIntMethod(display_dpi.obj(), getHeightMethod);

  // In the off chance we have non-square pixels, use the max ratio so the
  // highest quality video suitable to the device gets selected.
  size->video_pixel_ratio =
      std::max(static_cast<float>(display_width) / size->width,
               static_cast<float>(display_height) / size->height);

  return true;
}
