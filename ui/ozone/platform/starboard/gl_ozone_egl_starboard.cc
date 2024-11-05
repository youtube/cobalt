// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
  return gfx::Size(1280, 720);
}

}  // namespace

GLOzoneEglStarboard::GLOzoneEglStarboard()
    : display_size_(GetDisplaySize()) {
}

GLOzoneEglStarboard::~GLOzoneEglStarboard() {
  TerminateDisplay();
}

void GLOzoneEglStarboard::InitializeHardwareIfNeeded() {
  if (hardware_initialized_)
    return;

  hardware_initialized_ = true;
}

void GLOzoneEglStarboard::TerminateDisplay() {}

scoped_refptr<gl::GLSurface> GLOzoneEglStarboard::CreateViewGLSurface(
    gl::GLDisplay* display,
    gfx::AcceleratedWidget widget) {
  return gl::InitializeGLSurface(
      new GLSurfaceStarboard(display->GetAs<gl::GLDisplayEGL>(), widget, this));
}

scoped_refptr<gl::GLSurface> GLOzoneEglStarboard::CreateOffscreenGLSurface(
    gl::GLDisplay* display,
    const gfx::Size& size) {
  return gl::InitializeGLSurface(
      new gl::PbufferGLSurfaceEGL(display->GetAs<gl::GLDisplayEGL>(), size));
}

gl::EGLDisplayPlatform GLOzoneEglStarboard::GetNativeDisplay() {
  CreateDisplayTypeAndWindowIfNeeded();
  return gl::EGLDisplayPlatform(
      reinterpret_cast<EGLNativeDisplayType>(display_type_));
}

void GLOzoneEglStarboard::CreateDisplayTypeAndWindowIfNeeded() {
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
  CreateDisplayTypeAndWindowIfNeeded();
  return reinterpret_cast<intptr_t>(window_);
}

bool GLOzoneEglStarboard::ResizeDisplay(gfx::Size size) {
  return true;
}

bool GLOzoneEglStarboard::LoadGLES2Bindings(
    const gl::GLImplementationParts& implementation) {
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
