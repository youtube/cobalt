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

#ifndef COBALT_RENDERER_BACKEND_EGL_DISPLAY_H_
#define COBALT_RENDERER_BACKEND_EGL_DISPLAY_H_

#include <EGL/egl.h>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/renderer/backend/display.h"
#include "cobalt/renderer/backend/graphics_system.h"

namespace cobalt {
namespace renderer {
namespace backend {

class RenderTarget;

class DisplayEGL : public Display {
 public:
  // The DisplayEGL is constructed with a EGLSurface created from
  // a call to eglCreateWindowSurface().
  DisplayEGL(EGLDisplay display, EGLSurface surface);

  scoped_refptr<RenderTarget> GetRenderTarget() override;

 private:
  scoped_refptr<RenderTarget> render_target_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_DISPLAY_H_
