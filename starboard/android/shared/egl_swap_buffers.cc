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
#include <GLES2/gl2.h>

#include "starboard/android/shared/video_window.h"
#include "starboard/shared/gles/gl_call.h"

extern "C" {
EGLBoolean __real_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);

// This needs to be exported to ensure shared_library targets include it.
SB_EXPORT_PLATFORM EGLBoolean __wrap_eglSwapBuffers(
    EGLDisplay dpy, EGLSurface surface) {
  // Kick off the GPU while waiting for new player bounds to take effect.
  GL_CALL(glFlush());

  // Wait for player bounds to take effect before presenting the UI frame which
  // uses those player bounds. This helps to keep punch-out videos in sync with
  // the UI frame.
  // TODO: It is possible for the new UI frame to be displayed too late
  // (especially if there's a lot to render), so this case still needs to
  // be handled.

  // Note, we're no longer calling WaitForVideoBoundsUpdate because it does
  // not work properly without calling SurfaceHolder setFixedSize.
  // starboard::android::shared::WaitForVideoBoundsUpdate();

  return __real_eglSwapBuffers(dpy, surface);
}

}
