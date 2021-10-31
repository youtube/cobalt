/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "glimp/gles/texture.h"

#include "nb/pointer_arithmetic.h"
#include "nb/rect.h"

namespace glimp {
namespace gles {

Texture::Texture(nb::scoped_ptr<TextureImpl> impl)
    : impl_(impl.Pass()), texture_allocated_(false), bound_to_surface_(NULL) {}

void Texture::Initialize(GLint level,
                         PixelFormat pixel_format,
                         GLsizei width,
                         GLsizei height) {
  width_ = static_cast<int>(width);
  height_ = static_cast<int>(height);
  pixel_format_ = pixel_format;

  impl_->Initialize(level, pixel_format_, width_, height_);

  texture_allocated_ = true;
}

bool Texture::UpdateData(GLint level,
                         GLint xoffset,
                         GLint yoffset,
                         GLsizei width,
                         GLsizei height,
                         int pitch_in_bytes,
                         const GLvoid* pixels) {
  SB_DCHECK(pixels != NULL);
  return impl_->UpdateData(level,
                           nb::Rect<int>(xoffset, yoffset, width, height),
                           pitch_in_bytes, pixels);
}

void Texture::UpdateDataFromBuffer(
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    int pitch_in_bytes,
    const nb::scoped_refptr<Buffer>& pixel_unpack_buffer,
    uintptr_t buffer_offset) {
  SB_DCHECK(pixel_unpack_buffer);
  impl_->UpdateDataFromBuffer(
      level, nb::Rect<int>(xoffset, yoffset, width, height), pitch_in_bytes,
      pixel_unpack_buffer, buffer_offset);
}

bool Texture::BindToEGLSurface(egl::Surface* surface) {
  if (bound_to_surface_ != NULL) {
    SB_DLOG(WARNING) << "A EGLSurface is already bound to this texture.";
    return false;
  }

  width_ = surface->GetWidth();
  height_ = surface->GetHeight();

  SB_DCHECK(surface->GetTextureFormat() == EGL_TEXTURE_RGBA);
  pixel_format_ = kPixelFormatRGBA8;
  texture_allocated_ = true;

  impl_->BindToEGLSurface(surface);
  bound_to_surface_ = surface;

  return true;
}

bool Texture::ReleaseFromEGLSurface(egl::Surface* surface) {
  if (bound_to_surface_ != surface) {
    SB_DLOG(WARNING) << "Attempting to release a surface that this texture was "
                        "not bound to.";
    return false;
  }

  width_ = 0;
  height_ = 0;
  pixel_format_ = kPixelFormatInvalid;
  texture_allocated_ = false;
  bound_to_surface_ = NULL;

  impl_->Initialize(0, kPixelFormatInvalid, 0, 0);

  return true;
}

void Texture::ReadPixelsAsRGBA8(GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                int pitch_in_bytes,
                                GLvoid* pixels) {
  impl_->ReadPixelsAsRGBA8(nb::Rect<int>(x, y, width, height), pitch_in_bytes,
                           pixels);
}

bool Texture::CanBeAttachedToFramebuffer() const {
  return impl_->CanBeAttachedToFramebuffer();
}

}  // namespace gles
}  // namespace glimp
