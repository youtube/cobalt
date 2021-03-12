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

#ifndef GLIMP_GLES_TEXTURE_IMPL_H_
#define GLIMP_GLES_TEXTURE_IMPL_H_

#include "glimp/egl/surface.h"
#include "glimp/gles/buffer.h"
#include "glimp/gles/pixel_format.h"
#include "glimp/gles/shader.h"
#include "nb/rect.h"
#include "nb/ref_counted.h"

namespace glimp {
namespace gles {

class TextureImpl {
 public:
  virtual ~TextureImpl() {}

  // Specifies texture parameters necessary to allocate texture data within
  // this texture.  This method must be called before pixel data can be
  // provided to the texture via UpdateData*() methods.  If |width| == 0,
  // the texture should be placed in an uninitialized state where UpdateData*()
  // methods are invalid.
  // This method is called when glTexImage2D() is called.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
  virtual void Initialize(int level,
                          PixelFormat pixel_format,
                          int width,
                          int height) = 0;

  // Called when glTexImage2D() is called with non-null pixels, or when
  // glTexSubImage2D() or glCopyTexSubImage2D() is called.  Updates an already
  // allocated texture with new texture data provided by the client.  The
  // parameters of this method define a window within the existing texture
  // data of which should be updated with the provided (non-NULL) |pixels|.
  // The provided |pitch_in_bytes| refers to the input data pixel rows.
  // This function will return true if successful, false if memory allocation
  // failed.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexSubImage2D.xml
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCopyTexSubImage2D.xml
  virtual bool UpdateData(int level,
                          const nb::Rect<int>& window,
                          int pitch_in_bytes,
                          const void* pixels) = 0;

  // Similar to UpdateData() above, however the source of pixels in this case
  // is a buffer object, and an offset into it.  This method will be called
  // when glTexImage2D() or glTexSubImage2D() is called while a buffer is bound
  // to the GL_PIXEL_UNPACK_BUFFER target.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexSubImage2D.xml
  virtual void UpdateDataFromBuffer(
      int level,
      const nb::Rect<int>& window,
      int pitch_in_bytes,
      const nb::scoped_refptr<Buffer>& pixel_unpack_buffer,
      uintptr_t buffer_offset) = 0;

  // Called when eglBindTexImage() is called.  When this occurs, the texture
  // should configure itself to point at the same data as the passed in
  // egl::Surface is pointing to, so that the egl::Surface can effectively be
  // referenced as a texture.  This method should clear out any existing image
  // data in the texture when it is called.  When eglReleaseTexImage() is
  // called, TextureImpl::Initialize() will be called with |width| = 0 to reset
  // this texture.
  //   https://www.khronos.org/registry/egl/sdk/docs/man/html/eglBindTexImage.xhtml
  virtual void BindToEGLSurface(egl::Surface* surface) = 0;

  // This is called when glReadPixels() is called.  The
  virtual void ReadPixelsAsRGBA8(const nb::Rect<int>& window,
                                 int pitch_in_bytes,
                                 void* pixels) = 0;

  // Returns true if this texture is valid for use as a Framebuffer color
  // attachment.  In other words, this should return true if the texture can
  // be used as a render target.  This method is called when the function
  // glCheckFramebufferStatus() is called on a framebuffer that has this
  // texture attached to it.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCheckFramebufferStatus.xml
  virtual bool CanBeAttachedToFramebuffer() const = 0;

 private:
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_TEXTURE_IMPL_H_
