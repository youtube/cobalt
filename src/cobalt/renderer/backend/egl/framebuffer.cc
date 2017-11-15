// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/backend/egl/framebuffer.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/logging.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace backend {

FramebufferEGL::FramebufferEGL(GraphicsContextEGL* graphics_context,
                               const math::Size& size, GLenum color_format,
                               GLenum depth_format)
    : graphics_context_(graphics_context), size_(size), error_(false) {
  GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(graphics_context_);

  // Create the framebuffer object.
  glGenFramebuffers(1, &framebuffer_handle_);
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "Error creating new framebuffer.";
    error_ = true;
    return;
  }
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_handle_));

  // Create and attach a texture for color.
  GLuint color_handle = 0;
  glGenTextures(1, &color_handle);
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "Error creating new texture.";
    error_ = true;
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glDeleteFramebuffers(1, &framebuffer_handle_));
    return;
  }

  GL_CALL(glBindTexture(GL_TEXTURE_2D, color_handle));
  glTexImage2D(GL_TEXTURE_2D, 0, color_format, size_.width(), size_.height(), 0,
               color_format, GL_UNSIGNED_BYTE, 0);
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "Error allocating new texture backing for a framebuffer.";
    error_ = true;
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CALL(glDeleteTextures(1, &color_handle));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_CALL(glDeleteFramebuffers(1, &framebuffer_handle_));
    return;
  }

  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D, color_handle, 0));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  color_texture_.reset(new TextureEGL(graphics_context_, color_handle, size_,
      color_format, GL_TEXTURE_2D, base::Closure()));

  // Create and attach a depth buffer if requested.
  depthbuffer_handle_ = 0;
  if (depth_format != GL_NONE) {
    if (!CreateDepthAttachment(depth_format)) {
      LOG(ERROR) << "Error creating depth attachment for framebuffer.";
      error_ = true;
      GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
      GL_CALL(glDeleteFramebuffers(1, &framebuffer_handle_));
      color_texture_.reset();
      return;
    }
  }

  // Verify the framebuffer object is valid.
  DCHECK_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}

void FramebufferEGL::EnsureDepthBufferAttached(GLenum depth_format) {
  if (depthbuffer_handle_ == 0) {
    GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
        graphics_context_);

    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_handle_));
    CreateDepthAttachment(depth_format);

    // Verify the framebuffer object is valid.
    DCHECK_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER),
              GL_FRAMEBUFFER_COMPLETE);
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
  }
}

FramebufferEGL::~FramebufferEGL() {
  if (!error_) {
    GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
        graphics_context_);
    GL_CALL(glDeleteFramebuffers(1, &framebuffer_handle_));
    if (depthbuffer_handle_ != 0) {
      GL_CALL(glDeleteRenderbuffers(1, &depthbuffer_handle_));
    }
  }
}

bool FramebufferEGL::CreateDepthAttachment(GLenum depth_format) {
  glGenRenderbuffers(1, &depthbuffer_handle_);
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "Error creating depth buffer object.";
    return false;
  }
  GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer_handle_));
  glRenderbufferStorage(GL_RENDERBUFFER, depth_format, size_.width(),
                        size_.height());
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "Error allocating memory for depth buffer.";
    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
    GL_CALL(glDeleteRenderbuffers(1, &depthbuffer_handle_));
    return false;
  }
  GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
      GL_RENDERBUFFER, depthbuffer_handle_));
  GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

  return true;
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
