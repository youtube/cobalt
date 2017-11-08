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

#ifndef COBALT_RENDERER_BACKEND_EGL_FRAMEBUFFER_H_
#define COBALT_RENDERER_BACKEND_EGL_FRAMEBUFFER_H_

#include <GLES2/gl2.h>

#include "base/memory/scoped_ptr.h"
#include "cobalt/math/size.h"

namespace cobalt {
namespace renderer {
namespace backend {

class GraphicsContextEGL;
class TextureEGL;

// Framebuffer object that can be rendered to or used as a source texture.
class FramebufferEGL {
 public:
  // Create a framebuffer object with the given |color_format| and
  // |depth_format|. If |depth_format| is GL_NONE, then no depth buffer
  // will be used.
  FramebufferEGL(GraphicsContextEGL* graphics_context, const math::Size& size,
                 GLenum color_format, GLenum depth_format);
  ~FramebufferEGL();

  const math::Size& GetSize() const { return size_; }

  // Ensure the framebuffer has a depth buffer. If not, then create and
  // attach one with the specified format.
  void EnsureDepthBufferAttached(GLenum depth_format);

  // Return the color attachment for the framebuffer as a texture.
  TextureEGL* GetColorTexture() const { return color_texture_.get(); }

  // Return the framebuffer object's ID.
  GLuint gl_handle() const { return framebuffer_handle_; }

  // Returns true if an error occurred during construction of this framebuffer
  // (indicating that this object is invalid).
  bool CreationError() const { return error_; }

 private:
  bool CreateDepthAttachment(GLenum depth_format);

  GraphicsContextEGL* graphics_context_;
  math::Size size_;

  // The handle to the framebuffer that can be passed into OpenGL functions.
  GLuint framebuffer_handle_;

  // The handle for the depth buffer. Only used internally.
  GLuint depthbuffer_handle_;

  // The color component of the framebuffer object as a texture.
  scoped_ptr<TextureEGL> color_texture_;

  // Did an error occur during construction?
  bool error_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_FRAMEBUFFER_H_
