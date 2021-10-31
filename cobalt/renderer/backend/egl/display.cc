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

#include "cobalt/renderer/backend/egl/display.h"

#include "cobalt/configuration/configuration.h"
#include "cobalt/renderer/backend/egl/render_target.h"
#include "cobalt/renderer/egl_and_gles.h"

namespace cobalt {
namespace renderer {
namespace backend {

class DisplayRenderTargetEGL : public RenderTargetEGL {
 public:
  DisplayRenderTargetEGL(EGLDisplay display, EGLSurface surface);

  const math::Size& GetSize() const override;

  EGLSurface GetSurface() const override;

  bool IsWindowRenderTarget() const override { return true; }

  bool CreationError() override { return false; }

 private:
  ~DisplayRenderTargetEGL() override;

  EGLDisplay display_;

  EGLSurface surface_;

  math::Size size_;
};

DisplayRenderTargetEGL::DisplayRenderTargetEGL(EGLDisplay display,
                                               EGLSurface surface)
    : display_(display), surface_(surface) {
  if (configuration::Configuration::GetInstance()
          ->CobaltRenderDirtyRegionOnly()) {
    // Configure the surface to preserve contents on swap.
    EGLBoolean surface_attrib_set = EGL_CALL_SIMPLE(eglSurfaceAttrib(
        display_, surface_, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED));
    // NOTE: Must check eglGetError() to clear any error flags and also check
    // the return value of eglSurfaceAttrib since some implementations may not
    // set the error condition.
    content_preserved_on_swap_ =
        EGL_CALL_SIMPLE(eglGetError()) == EGL_SUCCESS &&
        surface_attrib_set == EGL_TRUE;
  }

  // Query and cache information about the surface now that we have created it.
  EGLint egl_surface_width;
  EGLint egl_surface_height;
  EGL_CALL_SIMPLE(
      eglQuerySurface(display_, surface_, EGL_WIDTH, &egl_surface_width));
  EGL_CALL_SIMPLE(
      eglQuerySurface(display_, surface_, EGL_HEIGHT, &egl_surface_height));
  size_.SetSize(egl_surface_width, egl_surface_height);
}

const math::Size& DisplayRenderTargetEGL::GetSize() const { return size_; }

DisplayRenderTargetEGL::~DisplayRenderTargetEGL() {
  EGL_CALL_SIMPLE(eglDestroySurface(display_, surface_));
}

EGLSurface DisplayRenderTargetEGL::GetSurface() const { return surface_; }

DisplayEGL::DisplayEGL(EGLDisplay display, EGLSurface surface) {
  // A display effectively just hosts a DisplayRenderTargetEGL.
  render_target_ = new DisplayRenderTargetEGL(display, surface);
}

scoped_refptr<RenderTarget> DisplayEGL::GetRenderTarget() {
  return render_target_;
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
