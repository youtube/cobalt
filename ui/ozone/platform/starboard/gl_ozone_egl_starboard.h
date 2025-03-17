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

#ifndef UI_OZONE_PLATFORM_STARBOARD_GL_OZONE_EGL_STARBOARD_H_
#define UI_OZONE_PLATFORM_STARBOARD_GL_OZONE_EGL_STARBOARD_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"

namespace ui {

class GLOzoneEGLStarboard : public GLOzoneEGL {
 public:
  GLOzoneEGLStarboard();
  GLOzoneEGLStarboard(const GLOzoneEGLStarboard&) = delete;
  GLOzoneEGLStarboard& operator=(const GLOzoneEGLStarboard&) = delete;

  ~GLOzoneEGLStarboard() override;

  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget window) override;
  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLDisplay* display,
      const gfx::Size& size) override;

 protected:
  gl::EGLDisplayPlatform GetNativeDisplay() override;
  bool LoadGLES2Bindings(
      const gl::GLImplementationParts& implementation) override;

 private:
  void CreateDisplayTypeIfNeeded();

  void* display_type_ = nullptr;
  bool have_display_type_ = false;
  void* window_ = nullptr;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_GL_OZONE_EGL_STARBOARD_H_
