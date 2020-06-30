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

#ifndef GLIMP_GLES_TEXTURE_H_
#define GLIMP_GLES_TEXTURE_H_

#include <GLES3/gl3.h>

#include "glimp/egl/surface.h"
#include "glimp/gles/buffer.h"
#include "glimp/gles/pixel_format.h"
#include "glimp/gles/sampler.h"
#include "glimp/gles/texture_impl.h"
#include "nb/ref_counted.h"
#include "nb/scoped_ptr.h"

namespace glimp {
namespace gles {

class Texture : public nb::RefCountedThreadSafe<Texture> {
 public:
  explicit Texture(nb::scoped_ptr<TextureImpl> impl);

  void Initialize(GLint level,
                  PixelFormat pixel_format,
                  GLsizei width,
                  GLsizei height);

  // Implements support for glTexSubImage2D() and glCopyTexSubImage2D().
  // This function will return true if successful, false if memory allocation
  // failed.
  bool UpdateData(GLint level,
                  GLint xoffset,
                  GLint yoffset,
                  GLsizei width,
                  GLsizei height,
                  int pitch_in_bytes,
                  const GLvoid* pixels);

  // Implements support for glTexSubImage2D() when data is supplied by a
  // GL_PIXEL_UNPACK_BUFFER.
  void UpdateDataFromBuffer(
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLsizei width,
      GLsizei height,
      int pitch_in_bytes,
      const nb::scoped_refptr<Buffer>& pixel_unpack_buffer,
      uintptr_t buffer_offset);

  // These methods will be called when eglBindTexImage() and
  // eglReleaseTexImage() are called.
  bool BindToEGLSurface(egl::Surface* surface);
  bool ReleaseFromEGLSurface(egl::Surface* surface);

  // Write a copy of the texture data into a window within the pointer specified
  // by |pixels|.
  void ReadPixelsAsRGBA8(GLint x,
                         GLint y,
                         GLsizei width,
                         GLsizei height,
                         int pitch_in_bytes,
                         GLvoid* pixels);

  // Returns true if this texture can be used as a framebuffer component.
  // Essentially, this function is asking whether we can render to the texture
  // or not.
  bool CanBeAttachedToFramebuffer() const;

  TextureImpl* impl() const { return impl_.get(); }

  // Returns whether the data has been set yet or not.
  bool texture_allocated() const { return texture_allocated_; }

  int width() const {
    SB_DCHECK(texture_allocated_);
    return width_;
  }

  int height() const {
    SB_DCHECK(texture_allocated_);
    return height_;
  }

  PixelFormat pixel_format() const {
    SB_DCHECK(texture_allocated_);
    return pixel_format_;
  }

  Sampler* sampler_parameters() { return &sampler_parameters_; }
  const Sampler* sampler_parameters() const { return &sampler_parameters_; }

 private:
  friend class nb::RefCountedThreadSafe<Texture>;
  ~Texture() {}

  nb::scoped_ptr<TextureImpl> impl_;

  // True if underlying texture data has been allocated yet or not (e.g.
  // will be true after glTexImage2D() is called.)
  bool texture_allocated_;

  // The width and height of the texture, in pixels.
  int width_;
  int height_;

  // The pixel format of the set data.
  PixelFormat pixel_format_;

  // Non-null if we are currently bound to an egl::Surface (e.g. from a
  // call to eglBindTexImage()).
  egl::Surface* bound_to_surface_;

  // Sampler parameters that are associated with this texture.
  Sampler sampler_parameters_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_TEXTURE_H_
