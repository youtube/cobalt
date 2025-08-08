// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(USE_EGL)
#include <EGL/egl.h>
#endif

#include "ui/gl/gl_bindings.h"

#if defined(USE_GLX)
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/glx.h"
#endif

#if defined(USE_EGL)
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface_egl.h"
#endif

namespace gl {

#if defined(USE_EGL)
void DisplayExtensionsEGL::UpdateConditionalExtensionSettings(
    EGLDisplay display) {
  // For the moment, only two extensions can be conditionally disabled
  // through GPU driver bug workarounds mechanism:
  //   EGL_KHR_fence_sync
  //   EGL_KHR_wait_sync

  // In theory it's OK to allow disabling other EGL extensions, as far as they
  // are not the ones used in GLSurfaceEGL::InitializeOneOff().

  std::string extensions(GetPlatformExtensions(display));
  extensions += " ";

  b_EGL_KHR_fence_sync =
      extensions.find("EGL_KHR_fence_sync ") != std::string::npos;
  b_EGL_KHR_wait_sync =
      extensions.find("EGL_KHR_wait_sync ") != std::string::npos;
}

// static
std::string DisplayExtensionsEGL::GetPlatformExtensions(EGLDisplay display) {
  if (display == EGL_NO_DISPLAY)
    return "";
  const char* str = eglQueryString(display, EGL_EXTENSIONS);
  return str ? std::string(str) : "";
}

// static
std::string ClientExtensionsEGL::GetClientExtensions() {
  const char* str = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  return str ? std::string(str) : "";
}
#endif

#if defined(USE_GLX)
std::string DriverGLX::GetPlatformExtensions() {
  auto* connection = x11::Connection::Get();
  const int screen = connection ? connection->DefaultScreenId() : 0;
  const char* str =
      glXQueryExtensionsString(connection->GetXlibDisplay(), screen);
  return str ? std::string(str) : "";
}
#endif

}  // namespace gl
