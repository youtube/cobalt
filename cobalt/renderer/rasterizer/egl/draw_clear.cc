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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/rasterizer/egl/draw_clear.h"

#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

DrawClear::DrawClear(GraphicsState* graphics_state, const BaseState& base_state,
                     const render_tree::ColorRGBA& clear_color)
    : DrawObject(base_state), clear_color_(GetDrawColor(clear_color)) {}

void DrawClear::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
}

void DrawClear::ExecuteRasterize(GraphicsState* graphics_state,
                                 ShaderProgramManager* program_manager) {
  graphics_state->Scissor(base_state_.scissor.x(), base_state_.scissor.y(),
                          base_state_.scissor.width(),
                          base_state_.scissor.height());

  graphics_state->Clear(clear_color_.r(), clear_color_.g(), clear_color_.b(),
                        clear_color_.a());
}

base::TypeId DrawClear::GetTypeId() const {
  return base::GetTypeId<DrawClear>();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
