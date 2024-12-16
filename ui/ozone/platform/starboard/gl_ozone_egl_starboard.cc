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

#include <memory>

#include "starboard/egl.h"
#include "ui/gfx/vsync_provider.h"

namespace {
// TODO(b/371272304): Currently targeting a fixed 60Hz. Make this dynamic based
// on device capabilities.
base::TimeDelta GetVSyncInterval() {
  return base::Seconds(1) / 60;
}

}  // namespace

namespace ui {

GLOzoneEGLStarboard::GLOzoneEGLStarboard() = default;

GLOzoneEGLStarboard::~GLOzoneEGLStarboard() {
  if (sb_window_) {
    SbWindowDestroy(sb_window_);
  }
}

scoped_refptr<gl::GLSurface> GLOzoneEGLStarboard::CreateViewGLSurface(
    gl::GLDisplay* display,
    gfx::AcceleratedWidget window) {
  // TODO(b/371272304): Verify widget dimensions match our expected display size
  // (likely full screen for Cobalt).
  return gl::InitializeGLSurface(new gl::NativeViewGLSurfaceEGL(
      display->GetAs<gl::GLDisplayEGL>(), GetNativeWindow(),
      std::make_unique<gfx::FixedVSyncProvider>(base::TimeTicks(),
                                                GetVSyncInterval())));
}
scoped_refptr<gl::GLSurface> GLOzoneEGLStarboard::CreateOffscreenGLSurface(
    gl::GLDisplay* display,
    const gfx::Size& size) {
  return gl::InitializeGLSurface(
      new gl::PbufferGLSurfaceEGL(display->GetAs<gl::GLDisplayEGL>(), size));
}

intptr_t GLOzoneEGLStarboard::GetNativeWindow() {
  CreateDisplayTypeAndWindowIfNeeded();
  return reinterpret_cast<intptr_t>(window_);
}

gl::EGLDisplayPlatform GLOzoneEGLStarboard::GetNativeDisplay() {
  CreateDisplayTypeAndWindowIfNeeded();
  return gl::EGLDisplayPlatform(
      reinterpret_cast<EGLNativeDisplayType>(display_type_));
}

bool GLOzoneEGLStarboard::LoadGLES2Bindings(
    const gl::GLImplementationParts& implementation) {
  DCHECK_EQ(implementation.gl, gl::kGLImplementationEGLGLES2)
      << "Not supported: " << implementation.ToString();
  // TODO(b/371272304): Initialize hardware here if needed.
  gl::GLGetProcAddressProc gl_proc = reinterpret_cast<gl::GLGetProcAddressProc>(
      SbGetEglInterface()->eglGetProcAddress);

  if (!gl_proc) {
    LOG(ERROR) << "GLOzoneEglStarboard::LoadGLES2Bindings no gl_proc";
    return false;
  }

  gl::SetGLGetProcAddressProc(gl_proc);
  return true;
}

void GLOzoneEGLStarboard::CreateDisplayTypeAndWindowIfNeeded() {
  // TODO(b/371272304): Initialize hardware here if needed.
  if (!have_display_type_) {
    display_type_ = reinterpret_cast<void*>(SB_EGL_DEFAULT_DISPLAY);
    have_display_type_ = true;
  }
  if (!window_) {
    SbWindowOptions options{};
    SbWindowSetDefaultOptions(&options);

    sb_window_ = SbWindowCreate(&options);
    window_ = SbWindowGetPlatformHandle(sb_window_);
  }

  CHECK(window_);
}

}  // namespace ui
