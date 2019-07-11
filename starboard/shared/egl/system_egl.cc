// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
#include <EGL/eglext.h>

#include "starboard/egl.h"

#if !defined(EGL_VERSION_1_0) || !defined(EGL_VERSION_1_1) || \
    !defined(EGL_VERSION_1_2) || !defined(EGL_VERSION_1_3) || \
    !defined(EGL_VERSION_1_4)
#error "EGL version must be >= 1.4"
#endif

namespace {

// Convenience functions that redirect to the intended function but "cast" the
// type of the SbEglNative*Type parameter into the desired type. Depending on
// the platform, the type of cast to use is different so either C-style casts or
// constructor-style casts are needed to work across platforms (or provide
// implementations for these functions for each platform).

SbEglBoolean SbEglCopyBuffers(SbEglDisplay dpy,
                              SbEglSurface surface,
                              SbEglNativePixmapType target) {
  return eglCopyBuffers(dpy, surface, (EGLNativePixmapType)target);
}

SbEglSurface SbEglCreatePixmapSurface(SbEglDisplay dpy,
                                      SbEglConfig config,
                                      SbEglNativePixmapType pixmap,
                                      const SbEglInt32* attrib_list) {
  return eglCreatePixmapSurface(dpy, config, (EGLNativePixmapType)pixmap,
                                attrib_list);
}

SbEglSurface SbEglCreateWindowSurface(SbEglDisplay dpy,
                                      SbEglConfig config,
                                      SbEglNativeWindowType win,
                                      const SbEglInt32* attrib_list) {
  return eglCreateWindowSurface(dpy, config, (EGLNativeWindowType)win,
                                attrib_list);
}

SbEglDisplay SbEglGetDisplay(SbEglNativeDisplayType display_id) {
  return eglGetDisplay((EGLNativeDisplayType)display_id);
}

const SbEglInterface g_sb_egl_interface = {
    &eglChooseConfig,
    &SbEglCopyBuffers,
    &eglCreateContext,
    &eglCreatePbufferSurface,
    &SbEglCreatePixmapSurface,
    &SbEglCreateWindowSurface,
    &eglDestroyContext,
    &eglDestroySurface,
    &eglGetConfigAttrib,
    &eglGetConfigs,
    &eglGetCurrentDisplay,
    &eglGetCurrentSurface,
    &SbEglGetDisplay,
    &eglGetError,
    &eglGetProcAddress,
    &eglInitialize,
    &eglMakeCurrent,
    &eglQueryContext,
    &eglQueryString,
    &eglQuerySurface,
    &eglSwapBuffers,
    &eglTerminate,
    &eglWaitGL,
    &eglWaitNative,
    &eglBindTexImage,
    &eglReleaseTexImage,
    &eglSurfaceAttrib,
    &eglSwapInterval,
    &eglBindAPI,
    &eglQueryAPI,
    &eglCreatePbufferFromClientBuffer,
    &eglReleaseThread,
    &eglWaitClient,
    &eglGetCurrentContext,

    nullptr,  // eglCreateSync
    nullptr,  // eglDestroySync
    nullptr,  // eglClientWaitSync
    nullptr,  // eglGetSyncAttrib
    nullptr,  // eglCreateImage
    nullptr,  // eglDestroyImage
    nullptr,  // eglGetPlatformDisplay
    nullptr,  // eglCreatePlatformWindowSurface
    nullptr,  // eglCreatePlatformPixmapSurface
    nullptr,  // eglWaitSync
};

}  // namespace

const SbEglInterface* SbGetEglInterface() {
  return &g_sb_egl_interface;
}
