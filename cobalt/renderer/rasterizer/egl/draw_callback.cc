// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/egl/draw_callback.h"

#include "base/basictypes.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "egl/generated_shader_impl.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

DrawCallback::DrawCallback(const base::Closure& rasterize_callback)
    : rasterize_callback_(rasterize_callback) {}

void DrawCallback::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {}

void DrawCallback::ExecuteRasterize(GraphicsState* graphics_state,
                                    ShaderProgramManager* program_manager) {
  if (!rasterize_callback_.is_null()) {
    rasterize_callback_.Run();
  }
}

base::TypeId DrawCallback::GetTypeId() const {
  return base::GetTypeId<DrawCallback>();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
