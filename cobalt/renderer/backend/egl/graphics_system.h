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

#ifndef COBALT_RENDERER_BACKEND_EGL_GRAPHICS_SYSTEM_H_
#define COBALT_RENDERER_BACKEND_EGL_GRAPHICS_SYSTEM_H_

#include <EGL/egl.h>

#include "base/optional.h"
#include "cobalt/renderer/backend/egl/resource_context.h"
#include "cobalt/renderer/backend/egl/texture_data.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/system_window/system_window.h"

namespace cobalt {
namespace renderer {
namespace backend {

// Returns a EGL-specific graphics system that is implemented via EGL and
// OpenGL ES.
class GraphicsSystemEGL : public GraphicsSystem {
 public:
  explicit GraphicsSystemEGL(system_window::SystemWindow* system_window);
  ~GraphicsSystemEGL() override;

  EGLDisplay GetDisplay() { return display_; }

  scoped_ptr<Display> CreateDisplay(
      system_window::SystemWindow* system_window) override;

  scoped_ptr<GraphicsContext> CreateGraphicsContext() override;

  scoped_ptr<TextureDataEGL> AllocateTextureData(const math::Size& size,
                                                 GLenum format);
  scoped_ptr<RawTextureMemoryEGL> AllocateRawTextureMemory(size_t size_in_bytes,
                                                           size_t alignment);

 private:
  EGLDisplay display_;
  EGLConfig config_;

  // Track the system window that the precreated window surface is
  // associated with.
  system_window::SystemWindow* system_window_;

  // Part of our config selection process involves testing to see if the config
  // can actually be used to create a surface.  If successful, we store the
  // created surface to avoid re-creating it when it is needed later.
  EGLSurface window_surface_;

  // A special graphics context which is used exclusively for mapping/unmapping
  // texture memory.
  base::optional<ResourceContext> resource_context_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_GRAPHICS_SYSTEM_H_
