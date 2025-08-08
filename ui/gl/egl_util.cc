// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/egl_util.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <EGL/egl.h>
#else
#include "third_party/khronos/EGL/egl.h"
#endif

// This needs to be after the EGL includes
#include "ui/gl/gl_bindings.h"

namespace ui {

const char* GetEGLErrorString(uint32_t error) {
  switch (error) {
    case EGL_SUCCESS:
      return "EGL_SUCCESS";
    case EGL_NOT_INITIALIZED:
      return "EGL_NOT_INITIALIZED";
    case EGL_BAD_ACCESS:
      return "EGL_BAD_ACCESS";
    case EGL_BAD_ALLOC:
      return "EGL_BAD_ALLOC";
    case EGL_BAD_ATTRIBUTE:
      return "EGL_BAD_ATTRIBUTE";
    case EGL_BAD_CONFIG:
      return "EGL_BAD_CONFIG";
    case EGL_BAD_CONTEXT:
      return "EGL_BAD_CONTEXT";
    case EGL_BAD_CURRENT_SURFACE:
      return "EGL_BAD_CURRENT_SURFACE";
    case EGL_BAD_DISPLAY:
      return "EGL_BAD_DISPLAY";
    case EGL_BAD_MATCH:
      return "EGL_BAD_MATCH";
    case EGL_BAD_NATIVE_PIXMAP:
      return "EGL_BAD_NATIVE_PIXMAP";
    case EGL_BAD_NATIVE_WINDOW:
      return "EGL_BAD_NATIVE_WINDOW";
    case EGL_BAD_PARAMETER:
      return "EGL_BAD_PARAMETER";
    case EGL_BAD_SURFACE:
      return "EGL_BAD_SURFACE";
    case EGL_CONTEXT_LOST:
      return "EGL_CONTEXT_LOST";
    default:
      return "UNKNOWN";
  }
}

// Returns the last EGL error as a string.
const char* GetLastEGLErrorString() {
  return GetEGLErrorString(eglGetError());
}

}  // namespace ui
