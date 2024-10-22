// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_STARBOARD_GL_SURFACE_STARBOARD_H_
#define UI_OZONE_PLATFORM_STARBOARD_GL_SURFACE_STARBOARD_H_

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface_egl.h"

namespace ui {

class GLOzoneEglStarboard;

class GLSurfaceStarboard : public gl::NativeViewGLSurfaceEGL {
 public:
  GLSurfaceStarboard(gl::GLDisplayEGL* display,
                gfx::AcceleratedWidget widget,
                GLOzoneEglStarboard* parent);

  GLSurfaceStarboard(const GLSurfaceStarboard&) = delete;
  GLSurfaceStarboard& operator=(const GLSurfaceStarboard&) = delete;

  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  EGLConfig GetConfig() override;
  int GetBufferCount() const override;

 protected:
  ~GLSurfaceStarboard() override;

  gfx::AcceleratedWidget widget_;
  raw_ptr<GLOzoneEglStarboard> parent_;
  bool uses_triple_buffering_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_GL_SURFACE_STARBOARD_H_
