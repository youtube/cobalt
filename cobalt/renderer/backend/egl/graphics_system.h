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

#ifndef RENDERER_BACKEND_EGL_GRAPHICS_SYSTEM_H_
#define RENDERER_BACKEND_EGL_GRAPHICS_SYSTEM_H_

#include <EGL/egl.h>

#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/system_window/system_window.h"

namespace cobalt {
namespace renderer {
namespace backend {

// Helper function that gets the handle of a system window.
// Each platform that uses an EGL graphics system must provide an
// implementation of this function.
EGLNativeWindowType GetHandleFromSystemWindow(
    system_window::SystemWindow* system_window);

// Returns a EGL-specific graphics system that is implemented via EGL and
// OpenGL ES.
class GraphicsSystemEGL : public GraphicsSystem {
 public:
  GraphicsSystemEGL();
  ~GraphicsSystemEGL() OVERRIDE;

  scoped_ptr<Display> CreateDisplay(
      system_window::SystemWindow* system_window) OVERRIDE;

  scoped_ptr<GraphicsContext> CreateGraphicsContext() OVERRIDE;

 private:
  EGLDisplay display_;
  EGLConfig config_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // RENDERER_BACKEND_EGL_GRAPHICS_SYSTEM_H_
