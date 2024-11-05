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

#include <stdint.h>
#include <memory>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/common/gl_ozone_egl.h"


namespace ui {

class GLOzoneEglStarboard : public GLOzoneEGL {
 public:
  explicit GLOzoneEglStarboard();
  GLOzoneEglStarboard(const GLOzoneEglStarboard&) = delete;
  GLOzoneEglStarboard& operator=(const GLOzoneEglStarboard&) = delete;

  ~GLOzoneEglStarboard() override;

  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLDisplay* display,
      const gfx::Size& size) override;
  gl::EGLDisplayPlatform GetNativeDisplay() override;
  bool LoadGLES2Bindings(
      const gl::GLImplementationParts& implementation) override;

  intptr_t GetNativeWindow();
  bool ResizeDisplay(gfx::Size viewport_size);
  void TerminateDisplay();

 private:
  void CreateDisplayTypeAndWindowIfNeeded();
  void InitializeHardwareIfNeeded();

  bool hardware_initialized_ = false;
  void* display_type_ = nullptr;
  bool have_display_type_ = false;
  void* window_ = nullptr;
  gfx::Size display_size_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_GL_OZONE_EGL_STARBOARD_H_
