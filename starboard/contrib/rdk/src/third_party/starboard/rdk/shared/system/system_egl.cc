//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
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

#include "third_party/starboard/rdk/shared/application_rdk.h"
#include "third_party/starboard/rdk/shared/log_override.h"

#include <cstring>
#include <mutex>
#include <cstring>
#include <essos-app.h>

#ifdef EGL_PLATFORM_WAYLAND_EXT
extern "C" EssAppPlatformDisplayType EssContextGetAppPlatformDisplayType( EssCtx *ctx ) __attribute__((weak));
#endif

#if !defined(EGL_VERSION_1_0) || !defined(EGL_VERSION_1_1) || \
    !defined(EGL_VERSION_1_2) || !defined(EGL_VERSION_1_3) || \
    !defined(EGL_VERSION_1_4)
#error "EGL version must be >= 1.4"
#endif

namespace {

#ifdef EGL_PLATFORM_WAYLAND_EXT
static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC gEglCreatePlatformWindowSurfaceEXT;
static PFNEGLGETPLATFORMDISPLAYEXTPROC gEglGetPlatformDisplayEXT;

bool isExtensionSupported(const char* extension_list, const char* extension) {
  int len = strlen(extension);
  const char* ptr = extension_list;
  while ((ptr = strstr(ptr, extension))) {
    if (ptr[len] == ' ' || ptr[len] == '\0')
      return true;
    ptr += len;
  }
  return false;
}

bool resolveEglPlatfromExtFns() {
  static std::once_flag flag;
  std::call_once(flag, [] {
    const char* extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!isExtensionSupported(extensions, "EGL_EXT_platform_wayland")) {
      SB_LOG(INFO) << "Wayland EGL platform extension is not supported. Supported extensions: " << extensions;
    }
    else {
      gEglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
      gEglCreatePlatformWindowSurfaceEXT = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
      if (!gEglGetPlatformDisplayEXT || !gEglCreatePlatformWindowSurfaceEXT) {
        if (!gEglGetPlatformDisplayEXT)
          SB_LOG(INFO) << "eglGetPlatformDisplayEXT is not available";
        if (!gEglCreatePlatformWindowSurfaceEXT)
          SB_LOG(INFO) << "eglCreatePlatformWindowSurfaceEXT is not available";
        gEglGetPlatformDisplayEXT = nullptr;
        gEglCreatePlatformWindowSurfaceEXT = nullptr;
      } else {
        SB_LOG(INFO) << "Successfully resolved EGL platform display extension functions.";
      }
    }
  });
  return gEglGetPlatformDisplayEXT && gEglCreatePlatformWindowSurfaceEXT;
}
#endif

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
  SbEglSurface result = EGL_NO_SURFACE;

#ifdef EGL_PLATFORM_WAYLAND_EXT
  if (gEglCreatePlatformWindowSurfaceEXT) {
    result = gEglCreatePlatformWindowSurfaceEXT(dpy, config, (void*)win,
                                                attrib_list);
    if (result == EGL_NO_SURFACE)
      SB_LOG(WARNING) << "eglCreatePlatformWindowSurfaceEXT failed, err: " << eglGetError();
  }
#endif

  if (result == EGL_NO_SURFACE) {
    result = eglCreateWindowSurface(dpy, config, (EGLNativeWindowType)win,
                                    attrib_list);
    if (result == EGL_NO_SURFACE)
      SB_LOG(ERROR) << "eglCreateWindowSurface failed, err: " << eglGetError();
  }

  return result;
}

SbEglDisplay SbEglGetDisplay(SbEglNativeDisplayType display_id) {
  NativeDisplayType display_type;
  EssCtx *ctx = third_party::starboard::rdk::shared::Application::Get()->GetEssCtx();

  if (EssContextGetEGLDisplayType(ctx, &display_type) == false) {
    SB_LOG(ERROR) << "EssContextGetEGLDisplayType failed! Going to try display_id=" << display_id << '.';
    display_type = reinterpret_cast<NativeDisplayType>(display_id);
  }

#ifdef EGL_PLATFORM_WAYLAND_EXT
  if (EssContextGetAppPlatformDisplayType == nullptr) {
    SB_LOG(INFO) << "'EssContextGetAppPlatformDisplayType' is not available. Fallback to eglGetDisplay.";
  }
  else if (EssContextGetAppPlatformDisplayType(ctx) != EssAppPlatformDisplayType_waylandExtension) {
    SB_LOG(INFO) << "Essos app platform display type is not 'WaylandExtension' ("
                 << EssContextGetAppPlatformDisplayType(ctx)
                 << " != "
                 << EssAppPlatformDisplayType_waylandExtension << ")."
                 << " Fallback to eglGetDisplay.";
  }
  else if (!resolveEglPlatfromExtFns()) {
    SB_LOG(INFO) << "eglGetPlatformDisplayEXT is not available or failed. Fallback to eglGetDisplay.";
  }
  else {
    SbEglDisplay result = gEglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, reinterpret_cast<EGLNativeDisplayType>(display_type), nullptr);
    if (result == EGL_NO_DISPLAY) {
      SB_LOG(ERROR) << "eglGetPlatformDisplayEXT returned EGL_NO_DISPLAY. Fallback to eglGetDisplay.";
      gEglGetPlatformDisplayEXT = nullptr;
      gEglCreatePlatformWindowSurfaceEXT = nullptr;
    } else {
      SB_LOG(INFO) << "Using display=" << result << ", returned by eglGetPlatformDisplayEXT.";
      return result;
    }
  }
#endif

  return eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(display_type));
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
