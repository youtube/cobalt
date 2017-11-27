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

#include "cobalt/renderer/backend/egl/display.h"

#include "cobalt/renderer/backend/egl/render_target.h"

namespace cobalt {
namespace renderer {
namespace backend {

class DisplayRenderTargetEGL : public RenderTargetEGL {
 public:
  // The DisplayRenderTargetEGL is constructed with a handle to a native
  // window to use as the render target.
  DisplayRenderTargetEGL(EGLDisplay display, EGLConfig config,
                         EGLNativeWindowType window_handle);

  const math::Size& GetSize() const override;

  EGLSurface GetSurface() const override;

  bool IsWindowRenderTarget() const override { return true; }

  bool CreationError() override { return false; }

 private:
  ~DisplayRenderTargetEGL() override;

  EGLDisplay display_;
  EGLConfig config_;

  EGLNativeWindowType native_window_;

  EGLSurface surface_;

  math::Size size_;
};

DisplayRenderTargetEGL::DisplayRenderTargetEGL(
    EGLDisplay display, EGLConfig config, EGLNativeWindowType window_handle)
    : display_(display), config_(config), native_window_(window_handle) {
  surface_ = eglCreateWindowSurface(display_, config_, native_window_, NULL);
  CHECK_EQ(EGL_SUCCESS, eglGetError());

#if defined(COBALT_RENDER_DIRTY_REGION_ONLY)
  // Configure the surface to preserve contents on swap.
  EGLBoolean surface_attrib_set =
      eglSurfaceAttrib(display_, surface_,
                       EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);
  // NOTE: Must check eglGetError() to clear any error flags and also check
  // the return value of eglSurfaceAttrib since some implementations may not
  // set the error condition.
  content_preserved_on_swap_ =
      eglGetError() == EGL_SUCCESS && surface_attrib_set == EGL_TRUE;
#endif  // #if defined(COBALT_RENDER_DIRTY_REGION_ONLY)

  // Query and cache information about the surface now that we have created it.
  EGLint egl_surface_width;
  EGLint egl_surface_height;
  eglQuerySurface(display_, surface_, EGL_WIDTH, &egl_surface_width);
  eglQuerySurface(display_, surface_, EGL_HEIGHT, &egl_surface_height);
  size_.SetSize(egl_surface_width, egl_surface_height);
}

const math::Size& DisplayRenderTargetEGL::GetSize() const { return size_; }

DisplayRenderTargetEGL::~DisplayRenderTargetEGL() {
  eglDestroySurface(display_, surface_);
}

EGLSurface DisplayRenderTargetEGL::GetSurface() const { return surface_; }

DisplayEGL::DisplayEGL(EGLDisplay display, EGLConfig config,
                       EGLNativeWindowType window_handle) {
  // A display effectively just hosts a DisplayRenderTargetEGL.
  render_target_ = new DisplayRenderTargetEGL(display, config, window_handle);
}

scoped_refptr<RenderTarget> DisplayEGL::GetRenderTarget() {
  return render_target_;
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
