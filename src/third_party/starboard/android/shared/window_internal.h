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

#ifndef STARBOARD_ANDROID_SHARED_WINDOW_INTERNAL_H_
#define STARBOARD_ANDROID_SHARED_WINDOW_INTERNAL_H_

//#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglplatform.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/native_activity.h>
#include <android/window.h>
#include <EGL/eglext.h>

#include "starboard/shared/internal_only.h"
#include "starboard/window.h"

struct SbWindowPrivate {
  SbWindowPrivate(EGLNativeWindowType window,
                  const SbWindowOptions* options);
  ~SbWindowPrivate();
  EGLNativeWindowType window;
};

#endif  // STARBOARD_ANDROID_SHARED_WINDOW_INTERNAL_H_
