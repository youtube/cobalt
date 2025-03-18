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

namespace ui {

GLOzoneEGLStarboard::GLOzoneEGLStarboard() = default;

GLOzoneEGLStarboard::~GLOzoneEGLStarboard() = default;

scoped_refptr<gl::GLSurface> GLOzoneEGLStarboard::CreateViewGLSurface(
    gl::GLDisplay* display,
    gfx::AcceleratedWidget window) {
  CHECK(window != gfx::kNullAcceleratedWidget);
  // TODO(b/371272304): Verify widget dimensions match our expected display size
  // (likely full screen for Cobalt).
  return gl::InitializeGLSurface(new gl::NativeViewGLSurfaceEGL(
      display->GetAs<gl::GLDisplayEGL>(), window, nullptr));
}

scoped_refptr<gl::GLSurface> GLOzoneEGLStarboard::CreateOffscreenGLSurface(
    gl::GLDisplay* display,
    const gfx::Size& size) {
  return gl::InitializeGLSurface(
      new gl::PbufferGLSurfaceEGL(display->GetAs<gl::GLDisplayEGL>(), size));
}

gl::EGLDisplayPlatform GLOzoneEGLStarboard::GetNativeDisplay() {
  CreateDisplayTypeIfNeeded();
  return gl::EGLDisplayPlatform(
      reinterpret_cast<EGLNativeDisplayType>(display_type_));
}

bool GLOzoneEGLStarboard::LoadGLES2Bindings(
    const gl::GLImplementationParts& implementation) {
  DCHECK_EQ(implementation.gl, gl::kGLImplementationEGLANGLE)
      << "Not supported: " << implementation.ToString();
  // TODO(b/371272304): call into LoadDefaultEGLGLES2Bindings instead and let
  // Angle load GLES and EGL.
  gl::GLGetProcAddressProc gl_proc = reinterpret_cast<gl::GLGetProcAddressProc>(
      SbGetEglInterface()->eglGetProcAddress);

  if (!gl_proc) {
    LOG(ERROR) << "GLOzoneEglStarboard::LoadGLES2Bindings no gl_proc";
    return false;
  }

  gl::SetGLGetProcAddressProc(gl_proc);
  return true;
}

void GLOzoneEGLStarboard::CreateDisplayTypeIfNeeded() {
  // TODO(b/371272304): Initialize hardware here if needed.
  if (!have_display_type_) {
    display_type_ = reinterpret_cast<void*>(SB_EGL_DEFAULT_DISPLAY);
    have_display_type_ = true;
  }
}

}  // namespace ui
