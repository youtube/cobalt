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

#ifndef COBALT_RENDERER_BACKEND_EGL_FRAMEBUFFER_RENDER_TARGET_H_
#define COBALT_RENDERER_BACKEND_EGL_FRAMEBUFFER_RENDER_TARGET_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/math/size.h"
#include "cobalt/renderer/backend/egl/framebuffer.h"
#include "cobalt/renderer/backend/egl/render_target.h"

namespace cobalt {
namespace renderer {
namespace backend {

// A framebuffer object wrapped in a RenderTarget interface. This allows
// an easier path to reading pixels from the render target.
class FramebufferRenderTargetEGL : public RenderTargetEGL {
 public:
  FramebufferRenderTargetEGL(GraphicsContextEGL* graphics_context,
                             const math::Size& size)
      : framebuffer_(graphics_context, size, GL_RGBA, GL_NONE) {}

  const math::Size& GetSize() const OVERRIDE { return framebuffer_.GetSize(); }

  // This render target does not have an EGLSurface.
  EGLSurface GetSurface() const OVERRIDE { return EGL_NO_SURFACE; }

  // This handle is suitable for use with glBindFramebuffer.
  intptr_t GetPlatformHandle() const OVERRIDE {
    return framebuffer_.gl_handle();
  }

  // Create a depth buffer for the render target if it doesn't already have one.
  void EnsureDepthBufferAttached(GLenum depth_format) {
    framebuffer_.EnsureDepthBufferAttached(depth_format);
  }

  // Get the color texture attachment of the framebuffer.
  TextureEGL* GetColorTexture() const {
    return framebuffer_.GetColorTexture();
  }

  bool CreationError() OVERRIDE { return framebuffer_.CreationError(); }

 private:
  ~FramebufferRenderTargetEGL() OVERRIDE {}

  FramebufferEGL framebuffer_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_FRAMEBUFFER_RENDER_TARGET_H_
