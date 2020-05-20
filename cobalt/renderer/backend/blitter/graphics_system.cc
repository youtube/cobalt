// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/backend/blitter/graphics_system.h"

#include "cobalt/renderer/backend/blitter/display.h"
#include "cobalt/renderer/backend/blitter/graphics_context.h"

#if SB_API_VERSION < 12 && SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace backend {

GraphicsSystemBlitter::GraphicsSystemBlitter() {
  device_ = SbBlitterCreateDefaultDevice();
}

GraphicsSystemBlitter::~GraphicsSystemBlitter() {
  SbBlitterDestroyDevice(device_);
}

std::unique_ptr<Display> GraphicsSystemBlitter::CreateDisplay(
    system_window::SystemWindow* system_window) {
  return std::unique_ptr<Display>(new DisplayBlitter(device_, system_window));
}

std::unique_ptr<GraphicsContext>
GraphicsSystemBlitter::CreateGraphicsContext() {
  return std::unique_ptr<GraphicsContext>(new GraphicsContextBlitter(this));
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION < 12 && SB_HAS(BLITTER)
