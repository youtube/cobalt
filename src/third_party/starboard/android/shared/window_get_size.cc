// Copyright 2016 Google Inc. All Rights Reserved.
// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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
#include "third_party/starboard/android/shared/application_android.h"

#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))

bool SbWindowGetSize(SbWindow window, SbWindowSize* size) {
  if(!SbWindowIsValid(window) || !window->window) {
    return false;
  }

  size->width = ANativeWindow_getWidth(window->window);
  size->height = ANativeWindow_getHeight(window->window);
  // @todo
  size->video_pixel_ratio = 0;

  LOGI("Window size: width = %d, height = %d\n", size->width, size->height);

  return true;
}
