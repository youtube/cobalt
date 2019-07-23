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

#include "glimp/gles/framebuffer.h"
#include "starboard/common/log.h"

namespace glimp {
namespace gles {

Framebuffer::Framebuffer() : color_attachment_surface_(NULL) {}

Framebuffer::Framebuffer(egl::Surface* surface)
    : color_attachment_surface_(surface) {
  SB_DCHECK(surface != NULL);
}

int Framebuffer::GetWidth() const {
  if (color_attachment_surface_) {
    return color_attachment_surface_->GetWidth();
  } else {
    SB_DCHECK(color_attachment_texture_);
    return color_attachment_texture_->width();
  }
}

int Framebuffer::GetHeight() const {
  if (color_attachment_surface_) {
    return color_attachment_surface_->GetHeight();
  } else {
    SB_DCHECK(color_attachment_texture_);
    return color_attachment_texture_->height();
  }
}

void Framebuffer::AttachTexture2D(const nb::scoped_refptr<Texture>& texture,
                                  int level) {
  SB_DCHECK(!color_attachment_surface_);

  SB_DCHECK(level == 0) << "Only level=0 is supported in glimp.";

  color_attachment_texture_ = texture;
}

GLenum Framebuffer::CheckFramebufferStatus() const {
  // If the default framebuffer is bound, then the framebuffer is guaranteed
  // to be complete.
  if (color_attachment_surface_) {
    return GL_FRAMEBUFFER_COMPLETE;
  }

  if (!color_attachment_texture_) {
    return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
  }

  if (!color_attachment_texture_->CanBeAttachedToFramebuffer()) {
    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
  }

  if (depth_attachment_) {
    if (depth_attachment_->format() != GL_DEPTH_COMPONENT16) {
      return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    }

    if (GetWidth() != depth_attachment_->width() ||
        GetHeight() != depth_attachment_->height()) {
      return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
    }
  }

  if (stencil_attachment_) {
    if (stencil_attachment_->format() != GL_STENCIL_INDEX8) {
      return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    }

    if (GetWidth() != stencil_attachment_->width() ||
        GetHeight() != stencil_attachment_->height()) {
      return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
    }
  }

  return GL_FRAMEBUFFER_COMPLETE;
}

void Framebuffer::UpdateColorSurface(egl::Surface* surface) {
  SB_DCHECK(!color_attachment_texture_);
  SB_DCHECK(color_attachment_surface_);
  color_attachment_surface_ = surface;
}

void Framebuffer::SetDepthAttachment(
    const nb::scoped_refptr<Renderbuffer>& depth_attachment) {
  depth_attachment_ = depth_attachment;
}

void Framebuffer::SetStencilAttachment(
    const nb::scoped_refptr<Renderbuffer>& stencil_attachment) {
  stencil_attachment_ = stencil_attachment;
}

}  // namespace gles
}  // namespace glimp
