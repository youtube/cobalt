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

#ifndef COBALT_RENDERER_BACKEND_GRAPHICS_SYSTEM_STUB_H_
#define COBALT_RENDERER_BACKEND_GRAPHICS_SYSTEM_STUB_H_

#include "cobalt/renderer/backend/display_stub.h"
#include "cobalt/renderer/backend/graphics_context_stub.h"
#include "cobalt/renderer/backend/graphics_system.h"

namespace cobalt {

namespace system_window {
class SystemWindow;
}

namespace renderer {
namespace backend {

// A graphics system that does almost nothing.
// This graphics system and all related stub components are useful for platforms
// where we don't care about having or dealing with graphics at all, and also
// for new platforms for which the graphics system has not yet been implemented.
class GraphicsSystemStub : public GraphicsSystem {
 public:
  scoped_ptr<Display> CreateDisplay(
      system_window::SystemWindow* system_window) override {
    UNREFERENCED_PARAMETER(system_window);
    return scoped_ptr<Display>(new DisplayStub());
  }

  scoped_ptr<GraphicsContext> CreateGraphicsContext() override {
    return scoped_ptr<GraphicsContext>(new GraphicsContextStub(this));
  }
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_GRAPHICS_SYSTEM_STUB_H_
