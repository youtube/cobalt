// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/starboard/gl_ozone_egl_starboard.h"

#include <EGL/egl.h>
#include <dlfcn.h>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/platform/starboard/gl_surface_starboard.h"

#include "starboard/egl.h"
#include "starboard/window.h"

namespace ui {

namespace {

typedef EGLDisplay (*EGLGetDisplayFn)(NativeDisplayType);
typedef EGLBoolean (*EGLTerminateFn)(EGLDisplay);

// Display resolution, set in browser process and passed by switches.
gfx::Size GetDisplaySize() {
/*
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  int width, height;

  if (base::StringToInt(
          cmd_line->GetSwitchValueASCII(switches::kCastInitialScreenWidth),
          &width) &&
      base::StringToInt(
          cmd_line->GetSwitchValueASCII(switches::kCastInitialScreenHeight),
          &height)) {
    return gfx::Size(width, height);
  }
*/
  LOG(WARNING) << "Unable to get initial screen resolution from command line,"
               << "using default 720p";
  return gfx::Size(1280, 720);
}

}  // namespace

GLOzoneEglStarboard::GLOzoneEglStarboard()
    : display_size_(GetDisplaySize()) {
  LOG(INFO) << "GLOzoneEglStarboard::GLOzoneEglStarboard";
}

GLOzoneEglStarboard::~GLOzoneEglStarboard() {
  LOG(INFO) << "GLOzoneEglStarboard::~GLOzoneEglStarboard";
  // eglTerminate must be called first on display before releasing resources
  // and shutting down hardware
  TerminateDisplay();
}

void GLOzoneEglStarboard::InitializeHardwareIfNeeded() {
  LOG(INFO) << "GLOzoneEglStarboard::InitializeHardwareIfNeeded";
  if (hardware_initialized_)
    return;

  hardware_initialized_ = true;
}

void GLOzoneEglStarboard::TerminateDisplay() {
  LOG(INFO) << "GLOzoneEglStarboard::TerminateDisplay";
}

scoped_refptr<gl::GLSurface> GLOzoneEglStarboard::CreateViewGLSurface(
    gl::GLDisplay* display,
    gfx::AcceleratedWidget widget) {
  LOG(INFO) << "GLOzoneEglStarboard::CreateViewGLSurface";
  // Verify requested widget dimensions match our current display size.
  //DCHECK_EQ(static_cast<int>(widget >> 16), display_size_.width());
  //DCHECK_EQ(static_cast<int>(widget & 0xffff), display_size_.height());

  return gl::InitializeGLSurface(
      new GLSurfaceStarboard(display->GetAs<gl::GLDisplayEGL>(), widget, this));
}

scoped_refptr<gl::GLSurface> GLOzoneEglStarboard::CreateOffscreenGLSurface(
    gl::GLDisplay* display,
    const gfx::Size& size) {
  LOG(INFO) << "GLOzoneEglStarboard::CreateOffscreenGLSurface";

  return gl::InitializeGLSurface(
      new gl::PbufferGLSurfaceEGL(display->GetAs<gl::GLDisplayEGL>(), size));
}

gl::EGLDisplayPlatform GLOzoneEglStarboard::GetNativeDisplay() {
  LOG(INFO) << "GLOzoneEglStarboard::GetNativeDisplay";
  CreateDisplayTypeAndWindowIfNeeded();
  return gl::EGLDisplayPlatform(
      reinterpret_cast<EGLNativeDisplayType>(display_type_));
}

void GLOzoneEglStarboard::CreateDisplayTypeAndWindowIfNeeded() {
  LOG(INFO) << "GLOzoneEglStarboard::CreateDisplayTypeAndWindowIfNeeded";
  InitializeHardwareIfNeeded();

  if (!have_display_type_) {
    display_type_ = reinterpret_cast<void*>(SB_EGL_DEFAULT_DISPLAY); 
    have_display_type_ = true;
  }
  if (!window_) {
    SbWindowOptions options{};

    SbWindowSetDefaultOptions(&options);

    options.name = "starboard";
    options.size.width = 1280;
    options.size.height = 720;

    SbWindow sb_window = SbWindowCreate(&options);
    window_ =  SbWindowGetPlatformHandle(sb_window);
    
    CHECK(window_);
  }
}

intptr_t GLOzoneEglStarboard::GetNativeWindow() {
  LOG(INFO) << "GLOzoneEglStarboard::GetNativeWindow";
  CreateDisplayTypeAndWindowIfNeeded();
  return reinterpret_cast<intptr_t>(window_);
}

bool GLOzoneEglStarboard::ResizeDisplay(gfx::Size size) {
  LOG(INFO) << "GLOzoneEglStarboard::ResizeDisplay";

  //DCHECK_EQ(size.width(), display_size_.width());
  //DCHECK_EQ(size.height(), display_size_.height());
  return true;
}

bool GLOzoneEglStarboard::LoadGLES2Bindings(
    const gl::GLImplementationParts& implementation) {
  LOG(INFO) << "GLOzoneEglStarboard::LoadGLES2Bindings";
  InitializeHardwareIfNeeded();



  gl::GLGetProcAddressProc gl_proc = reinterpret_cast<gl::GLGetProcAddressProc>(
      SbGetEglInterface()->eglGetProcAddress);

  if (!gl_proc) {
    LOG(ERROR) << "GLOzoneEglStarboard::LoadGLES2Bindings no gl_proc";
    return false;
  }


  gl::SetGLGetProcAddressProc(gl_proc);
  return true;
}

}  // namespace ui
