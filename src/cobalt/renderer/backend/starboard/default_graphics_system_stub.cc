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

#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/backend/graphics_system_stub.h"

namespace cobalt {
namespace renderer {
namespace backend {

std::unique_ptr<GraphicsSystem> CreateDefaultGraphicsSystem(
    system_window::SystemWindow* system_window) {
  return std::unique_ptr<GraphicsSystem>(new GraphicsSystemStub());
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
