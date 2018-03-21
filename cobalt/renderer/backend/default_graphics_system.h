// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_BACKEND_DEFAULT_GRAPHICS_SYSTEM_H_
#define COBALT_RENDERER_BACKEND_DEFAULT_GRAPHICS_SYSTEM_H_

#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/system_window/system_window.h"

namespace cobalt {
namespace renderer {
namespace backend {

// The implementation of this function should be platform specific, and will
// create and return a platform-specific graphics system object.  This function
// is expected to be the main entry-point into the graphics system for most
// Cobalt platforms.  A |system_window| can optionally be provided, in which
// case the returned graphics system will be guaranteed to be compatible with
// the window passed in.
scoped_ptr<GraphicsSystem> CreateDefaultGraphicsSystem(
    system_window::SystemWindow* system_window);

inline scoped_ptr<GraphicsSystem> CreateDefaultGraphicsSystem() {
  return CreateDefaultGraphicsSystem(nullptr);
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_DEFAULT_GRAPHICS_SYSTEM_H_
