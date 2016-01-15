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

#include "glimp/gles/buffer.h"
#include "glimp/gles/pixel_format.h"
#include "glimp/gles/shader.h"
#include "glimp/nb/rect.h"
#include "glimp/nb/ref_counted.h"

namespace glimp {
namespace gles {

class TextureImpl {
 public:
  virtual ~TextureImpl() {}

  // Internally allocates memory for a texture of the specified format, width
  // and height, for the specified mimap level.  If pixels is not NULL, the
  // pixel values contained are to be copied into the newly allocated texture
  // data memory.  This method is called when glTexImage2D() is called.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexImage2D.xml
  virtual void SetData(int level,
                       PixelFormat pixel_format,
                       int width,
                       int height,
                       int pitch_in_bytes,
                       const void* pixels) = 0;

  // Similar to SetData() above, however the source of pixels in this case
  // is a buffer object, and an offset into it.  This method will be called
  // when glTexImage2D() is called while a buffer is currently bound to
  // the GL_PIXEL_UNPACK_BUFFER target.
  //   https://www.khronos.org/opengles/sdk/docs/man3/html/glTexImage2D.xhtml
  //   https://www.khronos.org/opengles/sdk/docs/man3/html/glBindBuffer.xhtml
  virtual void SetDataFromBuffer(
      int level,
      PixelFormat pixel_format,
      int width,
      int height,
      int pitch_in_bytes,
      const nb::scoped_refptr<Buffer>& pixel_unpack_buffer,
      uintptr_t buffer_offset) = 0;

  // Called when glTexSubImage2D() is called.  Updates an already allocated
  // texture data with new texture data provided by the client.  The parameters
  // of this method define a window within the existing texture data of which
  // should be updated with the provided (non-NULL) |pixels|.  The provided
  // |pitch_in_bytes| refers to the input data pixel rows.
  virtual void UpdateData(int level,
                          const nb::Rect<int>& window,
                          int pitch_in_bytes,
                          const void* pixels) = 0;

  // Similar to UpdateData() above, however the source of pixels in this case
  // is a buffer object, and an offset into it.  This method will be called
  // when glTexSubImage2D() is called while a buffer is currently bound to
  // the GL_PIXEL_UNPACK_BUFFER target.
  virtual void UpdateDataFromBuffer(
      int level,
      const nb::Rect<int>& window,
      int pitch_in_bytes,
      const nb::scoped_refptr<Buffer>& pixel_unpack_buffer,
      uintptr_t buffer_offset) = 0;

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
