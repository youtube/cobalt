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
    : DrawPolyColor(base_state),
      include_scissor_first_vert_(-1),
      exclude_scissor_first_vert_(-1) {
  // Reserve space for the box shadow's spread and possible scissor rects.
  attributes_.reserve(18);

  // Box shadow's spread.
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

  // Include scissor only sets the depth of pixels inside the rect.
  SB_DCHECK(!include_scissor.IsEmpty());
  include_scissor_first_vert_ = static_cast<GLint>(attributes_.size());
  AddRect(include_scissor, 0);

  // Exclude scissor resets the depth of pixels inside the rect that were
  // touched by the include scissor rect.
  if (!exclude_scissor.IsEmpty()) {
    float old_depth = base_state_.depth;
    base_state_.depth = graphics_state->FarthestDepth();
    exclude_scissor_first_vert_ = static_cast<GLint>(attributes_.size());
    AddRect(exclude_scissor, 0);
    base_state_.depth = old_depth;
  }

  graphics_state->ReserveVertexData(
      attributes_.size() * sizeof(VertexAttributes));
}

void DrawRectShadowSpread::ExecuteOnscreenRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  size_t vert_count = attributes_.size();
  SetupShader(graphics_state, program_manager);

  // Include and exclude scissor rects are used to stencil the writable pixels
  // using the depth buffer. If a scissor rect has been specified, then only
  // pixels whose depth == base_state_.depth are writable by the main polygon.
  //
  // Since these scissor rects pollute the depth buffer, they should only be
  // used during the transparency pass. This ensures that all subsequent draws
  // are for objects with closer depth or objects that don't overlap these
  // pixels anyway.
  SB_DCHECK(graphics_state->IsBlendEnabled());
  SB_DCHECK(!graphics_state->IsDepthWriteEnabled());
  SB_DCHECK(graphics_state->IsDepthTestEnabled());

  // Set pixels in inclusive scissor rect to base_state_.depth.
  SB_DCHECK(include_scissor_first_vert_ >= 0);
  graphics_state->EnableDepthWrite();
  vert_count -= 4;
  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, include_scissor_first_vert_, 4));
  GL_CALL(glDepthFunc(GL_EQUAL));
  if (exclude_scissor_first_vert_ >= 0) {
    // Set pixels in exclusive scissor rect to farthest depth.
    vert_count -= 4;
    GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, exclude_scissor_first_vert_, 4));
  }
  graphics_state->DisableDepthWrite();

  // Draw the box shadow's spread.
  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, vert_count));

  // Restore normal depth test function.
  graphics_state->ResetDepthFunc();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
