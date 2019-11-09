// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/renderer/backend/default_graphics_system.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/blitter/graphics_system.h"
#if SB_API_VERSION >= SB_ALL_RENDERERS_REQUIRED_VERSION || SB_HAS(GLES2)
#include "cobalt/renderer/backend/egl/graphics_system.h"
#endif  // SB_API_VERSION >= SB_ALL_RENDERERS_REQUIRED_VERSION || SB_HAS(GLES2)
#include "cobalt/renderer/backend/graphics_system_stub.h"
#include "cobalt/system_window/system_window.h"

namespace cobalt {
namespace renderer {
namespace backend {

std::unique_ptr<GraphicsSystem> CreateDefaultGraphicsSystem(
    system_window::SystemWindow* system_window) {
#if SB_API_VERSION >= SB_ALL_RENDERERS_REQUIRED_VERSION
  if (SbGetGlesInterface()) {
    return std::unique_ptr<GraphicsSystem>(
        new GraphicsSystemEGL(system_window));
  } else if (SbBlitterIsBlitterSupported()) {
    SB_UNREFERENCED_PARAMETER(system_window);
    return std::unique_ptr<GraphicsSystem>(new GraphicsSystemBlitter());
  } else {
    SB_UNREFERENCED_PARAMETER(system_window);
    return std::unique_ptr<GraphicsSystem>(new GraphicsSystemStub());
  }
#else  // SB_API_VERSION >= SB_ALL_RENDERERS_REQUIRED_VERSION
#if SB_HAS(GLES2)
  return std::unique_ptr<GraphicsSystem>(new GraphicsSystemEGL(system_window));
#elif SB_HAS(BLITTER)
  SB_UNREFERENCED_PARAMETER(system_window);
  return std::unique_ptr<GraphicsSystem>(new GraphicsSystemBlitter());
#else
  SB_UNREFERENCED_PARAMETER(system_window);
  return std::unique_ptr<GraphicsSystem>(new GraphicsSystemStub());
#endif
#endif  // SB_API_VERSION >= SB_ALL_RENDERERS_REQUIRED_VERSION
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
