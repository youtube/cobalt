// Copyright 2018 Google Inc. All Rights Reserved.
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

#include <X11/Xlib.h>
#include "cobalt/renderer/backend/egl/display.h"

extern "C" EGLDisplay __real_eglGetDisplay(EGLNativeDisplayType native_display);
extern "C" EGLDisplay __wrap_eglGetDisplay(EGLNativeDisplayType native_display);

extern "C" EGLBoolean __real_eglTerminate(EGLDisplay display);
extern "C" EGLBoolean __wrap_eglTerminate(EGLDisplay display);

Display* native_display_;

extern "C" EGLDisplay
    __wrap_eglGetDisplay(EGLNativeDisplayType native_display) {
  native_display_ = XOpenDisplay(0);
  return __real_eglGetDisplay((EGLNativeDisplayType) native_display_);
}

extern "C" EGLBoolean __wrap_eglTerminate(EGLDisplay display) {
  EGLBoolean result = __real_eglTerminate(display);
  XCloseDisplay(native_display_);
  return result;
}
