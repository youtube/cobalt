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

#include "starboard/egl.h"
#include "ui/ozone/common/egl_util.h"

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
  void* native_display = SbGetNativeEGLDisplayType();
  if (native_display) {
    return gl::EGLDisplayPlatform(
        reinterpret_cast<EGLNativeDisplayType>(native_display));
  }
  return gl::EGLDisplayPlatform(EGL_DEFAULT_DISPLAY);
}

bool GLOzoneEGLStarboard::LoadGLES2Bindings(
    const gl::GLImplementationParts& implementation) {
  return LoadDefaultEGLGLES2Bindings(implementation);
}

}  // namespace ui
