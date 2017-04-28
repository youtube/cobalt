// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/egl/draw_rect_shadow_spread.h"

#include <GLES2/gl2.h>

#include "cobalt/renderer/backend/egl/utils.h"
#include "starboard/log.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

DrawRectShadowSpread::DrawRectShadowSpread(GraphicsState* graphics_state,
    const BaseState& base_state, const math::RectF& inner_rect,
    const math::RectF& outer_rect, const render_tree::ColorRGBA& color,
    const math::RectF& include_scissor, const math::RectF& exclude_scissor)
    : DrawDepthStencil(base_state) {
  // Reserve space for the box shadow's spread and possible scissor rects.
  attributes_.reserve(10 + MaxVertsNeededForStencil());

  // Draw the box shadow's spread. This is a triangle strip covering the area
  // between outer rect and inner rect.
  uint32_t color32 = GetGLRGBA(color * base_state_.opacity);
  AddVertex(outer_rect.x(), outer_rect.y(), color32);
  AddVertex(inner_rect.x(), inner_rect.y(), color32);
  AddVertex(outer_rect.right(), outer_rect.y(), color32);
  AddVertex(inner_rect.right(), inner_rect.y(), color32);
  AddVertex(outer_rect.right(), outer_rect.bottom(), color32);
  AddVertex(inner_rect.right(), inner_rect.bottom(), color32);
  AddVertex(outer_rect.x(), outer_rect.bottom(), color32);
  AddVertex(inner_rect.x(), inner_rect.bottom(), color32);
  AddVertex(outer_rect.x(), outer_rect.y(), color32);
  AddVertex(inner_rect.x(), inner_rect.y(), color32);

  AddStencil(include_scissor, exclude_scissor);

  graphics_state->ReserveVertexData(
      attributes_.size() * sizeof(VertexAttributes));
}

void DrawRectShadowSpread::ExecuteOnscreenRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  SetupShader(graphics_state, program_manager);
  DrawStencil(graphics_state);
  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, 10));
  UndoStencilState(graphics_state);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
