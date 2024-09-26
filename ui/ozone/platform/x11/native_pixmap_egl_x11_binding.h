// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_EGL_X11_BINDING_H_
#define UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_EGL_X11_BINDING_H_

#include <memory>

#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"

namespace gl {
class NativePixmapEGLX11BindingHelper;
}

namespace ui {

// A binding maintained between NativePixmap and GL Textures in Ozone that works
// within the context of X11.
class NativePixmapEGLX11Binding : public NativePixmapGLBinding {
 public:
  explicit NativePixmapEGLX11Binding(
      std::unique_ptr<gl::NativePixmapEGLX11BindingHelper> binding_helper,
      gfx::BufferFormat format);
  ~NativePixmapEGLX11Binding() override;

  static std::unique_ptr<NativePixmapGLBinding> Create(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::Size plane_size,
      GLenum target,
      GLuint texture_id);

  static bool CanImportNativeGLXPixmap();

  // NativePixmapGLBinding:
  GLuint GetInternalFormat() override;
  GLenum GetDataType() override;

 private:
  bool BindTexture(GLenum target, GLuint texture_id);

  // TODO(crbug.com/1412693): Fold the helper class into this class once
  // GLImageEGLPixmap no longer exists.
  std::unique_ptr<gl::NativePixmapEGLX11BindingHelper> binding_helper_;
  gfx::BufferFormat format_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_EGL_X11_BINDING_H_
