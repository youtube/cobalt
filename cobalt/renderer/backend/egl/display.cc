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

#include "cobalt/renderer/backend/egl/display.h"

#include "cobalt/renderer/backend/egl/render_target.h"
#include "cobalt/renderer/backend/surface_info.h"

namespace cobalt {
namespace renderer {
namespace backend {

class DisplayRenderTargetEGL : public RenderTargetEGL {
 public:
  // The DisplayRenderTargetEGL is constructed with a handle to a native
  // window to use as the render target.
  DisplayRenderTargetEGL(EGLDisplay display, EGLConfig config,
                         EGLNativeWindowType window_handle);

  const SurfaceInfo& GetSurfaceInfo() OVERRIDE;

  EGLSurface GetSurface() const OVERRIDE;

 private:
  ~DisplayRenderTargetEGL() OVERRIDE;

  EGLDisplay display_;
  EGLConfig config_;

  EGLNativeWindowType native_window_;

  EGLSurface surface_;

  SurfaceInfo surface_info_;
};

DisplayRenderTargetEGL::DisplayRenderTargetEGL(
    EGLDisplay display, EGLConfig config, EGLNativeWindowType window_handle)
    : display_(display), config_(config), native_window_(window_handle) {
  surface_ = eglCreateWindowSurface(display_, config_, native_window_, NULL);

  // Query and cache information about the surface now that we have created it.
  EGLint egl_surface_width;
  EGLint egl_surface_height;
  eglQuerySurface(display_, surface_, EGL_WIDTH, &egl_surface_width);
  eglQuerySurface(display_, surface_, EGL_HEIGHT, &egl_surface_height);
  // Querying for the texture format is unimplemented in Angle, so it is left
  // out for now and assumed to be EGL_TEXTURE_RGBA.
  surface_info_ = SurfaceInfo(math::Size(egl_surface_width, egl_surface_height),
                              SurfaceInfo::kFormatRGBA8);
}

const SurfaceInfo& DisplayRenderTargetEGL::GetSurfaceInfo() {
  return surface_info_;
}

DisplayRenderTargetEGL::~DisplayRenderTargetEGL() {}

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
