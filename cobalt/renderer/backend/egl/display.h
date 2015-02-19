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

#ifndef RENDERER_BACKEND_EGL_DISPLAY_H_
#define RENDERER_BACKEND_EGL_DISPLAY_H_

#include <EGL/egl.h>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/renderer/backend/display.h"

namespace cobalt {
namespace renderer {
namespace backend {

class RenderTarget;

class DisplayEGL : public Display {
 public:
  // create_window_cb is required and called when a DisplayEGL object is
  // constructed, and the window it returns will be setup as the EGL
  // onscreen rendering target.  The destroy_window_cb callback will be
  // called when the on screen render target is destructed and we no longer
  // reference the created EGLNativeWindowType object.
  DisplayEGL(
      EGLDisplay display, EGLConfig config,
      const base::Callback<EGLNativeWindowType(void)>& create_window_cb,
      const base::Callback<void(EGLNativeWindowType)>& destroy_window_cb);

  scoped_refptr<RenderTarget> GetRenderTarget() OVERRIDE;

 private:
  scoped_refptr<RenderTarget> render_target_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // RENDERER_BACKEND_EGL_DISPLAY_H_
