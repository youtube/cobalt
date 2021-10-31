// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/backend/egl/pbuffer_render_target.h"

#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"

namespace cobalt {
namespace renderer {
namespace backend {

PBufferRenderTargetEGL::PBufferRenderTargetEGL(EGLDisplay display,
                                               EGLConfig config,
                                               const math::Size& dimensions)
    : display_(display), config_(config), size_(dimensions) {
  EGLint surface_attrib_list[] = {
      EGL_WIDTH, dimensions.width(),
      EGL_HEIGHT, dimensions.height(),
      EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
      EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
      EGL_NONE,
  };
  // When a dummy 0-area render target is requested, it will not be used as a
  // drawing target and we therefore don't require for it to be a texture.
  if (dimensions.height() == 0 && dimensions.width() == 0) {
    // Fake 1x1 to avoid an arithmetic error (SIGFPE) in eglMakeCurrent() on
    // some platforms.
    surface_attrib_list[1] = surface_attrib_list[3] = 1;
    surface_attrib_list[4] = EGL_NONE;
  }

  surface_ = EGL_CALL_SIMPLE(
      eglCreatePbufferSurface(display_, config_, surface_attrib_list));
  if (EGL_CALL_SIMPLE(eglGetError()) != EGL_SUCCESS) {
    surface_ = EGL_NO_SURFACE;
  }
}

const math::Size& PBufferRenderTargetEGL::GetSize() const { return size_; }

EGLSurface PBufferRenderTargetEGL::GetSurface() const {
  return surface_;
}

PBufferRenderTargetEGL::~PBufferRenderTargetEGL() {
  if (surface_ != EGL_NO_SURFACE) {
    EGL_CALL(eglDestroySurface(display_, surface_));
  }
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
