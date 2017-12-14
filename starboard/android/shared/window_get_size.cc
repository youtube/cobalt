// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/window_internal.h"
#include "starboard/log.h"
#include "starboard/window.h"

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;

bool SbWindowGetSize(SbWindow window, SbWindowSize* size) {
  if (!SbWindowIsValid(window)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid window.";
    return false;
  }

  size->width = ANativeWindow_getWidth(window->native_window);
  size->height = ANativeWindow_getHeight(window->native_window);

  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> display_size(
      env->CallStarboardObjectMethodOrAbort("getDisplaySize",
                                           "()Landroid/util/Size;"));
  int display_width =
      env->CallIntMethodOrAbort(display_size.Get(), "getWidth", "()I");
  int display_height =
      env->CallIntMethodOrAbort(display_size.Get(), "getHeight", "()I");

  // In the off chance we have non-square pixels, use the max ratio so the
  // highest quality video suitable to the device gets selected.
  size->video_pixel_ratio = std::max(
      static_cast<float>(display_width) / size->width,
      static_cast<float>(display_height) / size->height);

  return true;
}
