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

#include <atomic>
#include <string>

#include "starboard/common/log.h"
#include "starboard/egl.h"

#if !defined(EGL_VERSION_1_0) || !defined(EGL_VERSION_1_1) || \
    !defined(EGL_VERSION_1_2) || !defined(EGL_VERSION_1_3) || \
    !defined(EGL_VERSION_1_4)
#error "EGL version must be >= 1.4"
#endif

namespace {

std::atomic<int> g_open_surfaces_count{0};
std::atomic<int> g_open_contexts_count{0};

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

SbEglSurface SbEglCreatePbufferSurface(SbEglDisplay dpy,
                                       SbEglConfig config,
                                       const SbEglInt32* attrib_list) {
  SbEglSurface result = eglCreatePbufferSurface(dpy, config, attrib_list);
  if (result != EGL_NO_SURFACE) {
    int count = ++g_open_surfaces_count;
    SB_LOG(INFO) << "[Starboard EGL] Created Pbuffer Surface: " << result
                 << ", active surfaces: " << count;
  } else {
    SB_LOG(ERROR) << "[Starboard EGL] Failed to create Pbuffer Surface, err: "
                  << eglGetError();
  }
  return result;
}

SbEglSurface SbEglCreatePixmapSurface(SbEglDisplay dpy,
                                      SbEglConfig config,
                                      SbEglNativePixmapType pixmap,
                                      const SbEglInt32* attrib_list) {
  SbEglSurface result = eglCreatePixmapSurface(
      dpy, config, (EGLNativePixmapType)pixmap, attrib_list);
  if (result != EGL_NO_SURFACE) {
    int count = ++g_open_surfaces_count;
    SB_LOG(INFO) << "[Starboard EGL] Created Pixmap Surface: " << result
                 << ", active surfaces: " << count;
  } else {
    SB_LOG(ERROR) << "[Starboard EGL] Failed to create Pixmap Surface, err: "
                  << eglGetError();
  }
  return result;
}

SbEglSurface SbEglCreateWindowSurface(SbEglDisplay dpy,
                                      SbEglConfig config,
                                      SbEglNativeWindowType win,
                                      const SbEglInt32* attrib_list) {
  SbEglSurface result = eglCreateWindowSurface(
      dpy, config, (EGLNativeWindowType)win, attrib_list);
  if (result != EGL_NO_SURFACE) {
    int count = ++g_open_surfaces_count;
    SB_LOG(INFO) << "[Starboard EGL] Created Window Surface: " << result
                 << " (win=" << reinterpret_cast<void*>(win)
                 << "), active surfaces: " << count;
  } else {
    SB_LOG(ERROR) << "[Starboard EGL] Failed to create Window Surface, err: "
                  << eglGetError();
  }
  return result;
}

SbEglBoolean SbEglDestroySurface(SbEglDisplay dpy, SbEglSurface surface) {
  SbEglBoolean result = eglDestroySurface(dpy, surface);
  if (result) {
    int count = --g_open_surfaces_count;
    SB_LOG(INFO) << "[Starboard EGL] Destroyed Surface: " << surface
                 << ", active surfaces: " << count;
  } else {
    SB_LOG(ERROR) << "[Starboard EGL] Failed to destroy Surface: " << surface
                  << ", err: " << eglGetError();
  }
  return result;
}

SbEglContext SbEglCreateContext(SbEglDisplay dpy,
                                SbEglConfig config,
                                SbEglContext share_context,
                                const SbEglInt32* attrib_list) {
  SbEglContext result =
      eglCreateContext(dpy, config, share_context, attrib_list);
  if (result != EGL_NO_CONTEXT) {
    int count = ++g_open_contexts_count;
    SB_LOG(INFO) << "[Starboard EGL] Created Context: " << result
                 << " (share=" << share_context
                 << "), active contexts: " << count;
  } else {
    SB_LOG(ERROR) << "[Starboard EGL] Failed to create Context, err: "
                  << eglGetError();
  }
  return result;
}

SbEglBoolean SbEglDestroyContext(SbEglDisplay dpy, SbEglContext ctx) {
  SbEglBoolean result = eglDestroyContext(dpy, ctx);
  if (result) {
    int count = --g_open_contexts_count;
    SB_LOG(INFO) << "[Starboard EGL] Destroyed Context: " << ctx
                 << ", active contexts: " << count;
  } else {
    SB_LOG(ERROR) << "[Starboard EGL] Failed to destroy Context: " << ctx
                  << ", err: " << eglGetError();
  }
  return result;
}

SbEglBoolean SbEglInitialize(SbEglDisplay dpy,
                             SbEglInt32* major,
                             SbEglInt32* minor) {
  SbEglBoolean result = eglInitialize(dpy, major, minor);
  SB_LOG(INFO) << "[Starboard EGL] eglInitialize on display: " << dpy
               << ", result: " << (result ? "SUCCESS" : "FAILED");
  return result;
}

SbEglBoolean SbEglTerminate(SbEglDisplay dpy) {
  SbEglBoolean result = eglTerminate(dpy);
  SB_LOG(INFO) << "[Starboard EGL] eglTerminate on display: " << dpy
               << ", result: " << (result ? "SUCCESS" : "FAILED");
  return result;
}

SbEglDisplay SbEglGetDisplay(SbEglNativeDisplayType display_id) {
  return eglGetDisplay((EGLNativeDisplayType)display_id);
}

#if defined(EGL_VERSION_1_5) && !BUILDFLAG(IS_ANDROID)
SbEglDisplay SbEglGetPlatformDisplay(SbEglEnum platform,
                                     void* native_display,
                                     const SbEglAttrib* attrib_list) {
  // TODO: Revisit adapter and add a provision to crash or handle cases
  // where attrib_list contains pointers too large to be converted to EGLAttrib.
  return eglGetPlatformDisplay(platform, native_display,
                               reinterpret_cast<const EGLAttrib*>(attrib_list));
}
#endif  // defined EGL_VERSION_1_5

const SbEglInterface g_sb_egl_interface = {
    &eglChooseConfig,
    &SbEglCopyBuffers,
    &SbEglCreateContext,
    &SbEglCreatePbufferSurface,
    &SbEglCreatePixmapSurface,
    &SbEglCreateWindowSurface,
    &SbEglDestroyContext,
    &SbEglDestroySurface,
    &eglGetConfigAttrib,
    &eglGetConfigs,
    &eglGetCurrentDisplay,
    &eglGetCurrentSurface,
    &SbEglGetDisplay,
    &eglGetError,
    &eglGetProcAddress,
    &SbEglInitialize,
    &eglMakeCurrent,
    &eglQueryContext,
    &eglQueryString,
    &eglQuerySurface,
    &eglSwapBuffers,
    &SbEglTerminate,
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
#if BUILDFLAG(IS_ANDROID) || !defined(EGL_VERSION_1_5)
    nullptr,  // eglGetPlatformDisplay
#else
    &SbEglGetPlatformDisplay,
#endif        // BUILDFLAG(IS_ANDROID) || !defined(EGL_VERSION_1_5)
    nullptr,  // eglCreatePlatformWindowSurface
    nullptr,  // eglCreatePlatformPixmapSurface
    nullptr,  // eglWaitSync
};

}  // namespace

const SbEglInterface* SbGetEglInterface() {
  return &g_sb_egl_interface;
}
