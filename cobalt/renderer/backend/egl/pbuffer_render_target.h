// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_BACKEND_EGL_PBUFFER_RENDER_TARGET_H_
#define COBALT_RENDERER_BACKEND_EGL_PBUFFER_RENDER_TARGET_H_

#include <EGL/egl.h>

#include "cobalt/math/size.h"
#include "cobalt/renderer/backend/egl/render_target.h"

namespace cobalt {
namespace renderer {
namespace backend {

// A PBuffer render target provides an encapsulation around an EGL PBO
// (Pixel Buffer Object) which can be used as an offscreen rendering target.
class PBufferRenderTargetEGL : public RenderTargetEGL {
 public:
  PBufferRenderTargetEGL(
      EGLDisplay display, EGLConfig config, const math::Size& dimensions);

  const math::Size& GetSize() const override;

  EGLSurface GetSurface() const override;

  EGLDisplay display() const { return display_; }
  EGLConfig config() const { return config_; }

  bool CreationError() override { return surface_ == EGL_NO_SURFACE; }

 private:
  ~PBufferRenderTargetEGL() override;

  EGLDisplay display_;
  EGLConfig config_;
  EGLSurface surface_;

  math::Size size_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_PBUFFER_RENDER_TARGET_H_
