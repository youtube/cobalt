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

#ifndef STARBOARD_SHARED_UWP_WINDOW_INTERNAL_H_
#define STARBOARD_SHARED_UWP_WINDOW_INTERNAL_H_

#include <EGL/egl.h>

#include "starboard/atomic.h"
#include "starboard/time.h"
#include "starboard/window.h"

struct SbWindowPrivate {
  SbWindowPrivate(int width, int height);
  ~SbWindowPrivate();

  EGLNativeWindowType egl_native_window() const {
    return reinterpret_cast<EGLNativeWindowType>(angle_property_set);
  }

  // The width of this window.
  int width;

  // The height of this window.
  int height;

 private:
  Windows::Foundation::Collections::PropertySet^ angle_property_set;
};

#endif  // STARBOARD_SHARED_UWP_WINDOW_INTERNAL_H_
