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

#include <EGL/egl.h>
#include "starboard/shared/x11/application_x11.h"

extern "C" {
EGLBoolean __real_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);

EGLBoolean __wrap_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
  // This function is a proxy for when the GL layer is updated. Unfortunately,
  // OpenGL ES 2.0 / X11 do not provide interfaces to know or control exactly
  // when the GL layer is updated, but with an eglSwapInterval of 0, this is
  // a good approximation.
  EGLBoolean return_value;
  starboard::shared::x11::ApplicationX11* application =
      starboard::shared::x11::ApplicationX11::Get();

  // Notify the application when the swap happens so that it can synchronize
  // the bounds for the video layer.
  application->SwapBuffersBegin();
  return_value = __real_eglSwapBuffers(dpy, surface);
  application->SwapBuffersEnd();
  return return_value;
}
}
