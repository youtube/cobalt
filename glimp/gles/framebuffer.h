/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef GLIMP_GLES_FRAMEBUFFER_H_
#define GLIMP_GLES_FRAMEBUFFER_H_

#include "glimp/egl/surface.h"
#include "glimp/gles/renderbuffer.h"
#include "glimp/gles/texture.h"
#include "nb/ref_counted.h"

namespace glimp {
namespace gles {

// Framebuffers represent GL framebuffer objects.  They are generated via
// calls to glGenFramebuffers().  Framebuffers represent the collection of
// components required for draw calls to direct their output towards.  These
// components are called "attachments" and include the color buffer,
// depth buffer and stencil buffer.  These different buffers can be attached
// by various GL calls.  For example, to attach a color buffer, one should
// call glFramebufferTexture2D() and pass in a GL texture object.
//   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glGenFramebuffers.xml
class Framebuffer : public nb::RefCountedThreadSafe<Framebuffer> {
 public:
  Framebuffer();

  // This constructor can be used to initialize the default Framebuffer.  This
  // framebuffer cannot later have separate attachments made to it.
  explicit Framebuffer(egl::Surface* surface);

  // Returns the attached color buffer's width and height.
  int GetWidth() const;
  int GetHeight() const;

  // Called when glFramebufferTexture2D() is called.  Will attach the specified
  // texture to this framebuffer.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glFramebufferTexture2D.xml
  void AttachTexture2D(const nb::scoped_refptr<Texture>& texture, int level);

  // Called when glCheckFramebufferStatus() is called.  This will check that
  // that there are not no components attached, and that all attached components
  // are consistent with each other and valid.  If everything checks out,
  // GL_FRAMEBUFFER_COMPLETE is returned.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCheckFramebufferStatus.xml
  GLenum CheckFramebufferStatus() const;

  // Called by eglMakeCurrent() on the default framebuffer in order to update
  // the egl::Surface* on the color attachment.  This can only be called on
  // a surface constructed with a egl::Surface* parameter (e.g. the default
  // framebuffer).
  void UpdateColorSurface(egl::Surface* surface);

  void SetDepthAttachment(const nb::scoped_refptr<Renderbuffer>& depth_buffer);
  void SetStencilAttachment(
      const nb::scoped_refptr<Renderbuffer>& stencil_buffer);

  const nb::scoped_refptr<Texture>& color_attachment_texture() const {
    return color_attachment_texture_;
  }

  egl::Surface* color_attachment_surface() const {
    return color_attachment_surface_;
  }

  const nb::scoped_refptr<Renderbuffer>& depth_attachment() const {
    return depth_attachment_;
  }

  const nb::scoped_refptr<Renderbuffer>& stencil_attachment() const {
    return stencil_attachment_;
  }

 private:
  friend class nb::RefCountedThreadSafe<Framebuffer>;
  ~Framebuffer() {}

  // Only one of |color_attachment_texture_| or |color_attachment_surface_|
  // can be non-NULL.  If |color_attachment_surface_| is non-NULL, then this
  // is the default framebuffer.
  nb::scoped_refptr<Texture> color_attachment_texture_;
  egl::Surface* color_attachment_surface_;

  nb::scoped_refptr<Renderbuffer> depth_attachment_;
  nb::scoped_refptr<Renderbuffer> stencil_attachment_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_FRAMEBUFFER_H_
