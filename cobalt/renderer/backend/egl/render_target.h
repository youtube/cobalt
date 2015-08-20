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

#ifndef RENDERER_BACKEND_EGL_RENDER_TARGET_H_
#define RENDERER_BACKEND_EGL_RENDER_TARGET_H_

#include <EGL/egl.h>

#include "cobalt/renderer/backend/render_target.h"

namespace cobalt {
namespace renderer {
namespace backend {

class RenderTargetEGL : public RenderTarget {
 public:
  // An EGLSurface is needed for the EGL function eglMakeCurrent() which
  // associates a render target with a rendering context.
  virtual EGLSurface GetSurface() const = 0;

  // Since all RenderTargets defined at this level are EGL objects, they
  // will always be set via eglMakeCurrent() and so they can always be
  // referenced by OpenGL by binding framebuffer 0.
  intptr_t GetPlatformHandle() OVERRIDE { return 0; }

 protected:
  virtual ~RenderTargetEGL() {}
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // RENDERER_BACKEND_EGL_RENDER_TARGET_H_
