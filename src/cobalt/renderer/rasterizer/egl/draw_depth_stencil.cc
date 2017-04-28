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

#include "cobalt/renderer/rasterizer/egl/draw_depth_stencil.h"

#include <GLES2/gl2.h>

#include "cobalt/renderer/backend/egl/utils.h"
#include "starboard/log.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

DrawDepthStencil::DrawDepthStencil(GraphicsState* graphics_state,
    const BaseState& base_state, const math::RectF& include_scissor,
    const math::RectF& exclude_scissor)
    : DrawPolyColor(base_state),
      include_scissor_first_vert_(-1),
      exclude_scissor_first_vert_(-1) {
  attributes_.reserve(MaxVertsNeededForStencil());
  AddStencil(include_scissor, exclude_scissor);
  graphics_state->ReserveVertexData(
      attributes_.size() * sizeof(VertexAttributes));
}

DrawDepthStencil::DrawDepthStencil(const BaseState& base_state)
    : DrawPolyColor(base_state),
      include_scissor_first_vert_(-1),
      exclude_scissor_first_vert_(-1) {
}

void DrawDepthStencil::ExecuteOnscreenRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  SetupShader(graphics_state, program_manager);
  DrawStencil(graphics_state);
}

void DrawDepthStencil::UndoStencilState(GraphicsState* graphics_state) {
  graphics_state->ResetDepthFunc();
}

void DrawDepthStencil::DrawStencil(GraphicsState* graphics_state) {
  // This must occur during the transparency pass.
  SB_DCHECK(graphics_state->IsBlendEnabled());
  SB_DCHECK(!graphics_state->IsDepthWriteEnabled());
  SB_DCHECK(graphics_state->IsDepthTestEnabled());

  // Set depth of pixels in the scissor rects.
  graphics_state->EnableDepthWrite();
  if (exclude_scissor_first_vert_ >= 0) {
    GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, exclude_scissor_first_vert_, 4));
  }
  SB_DCHECK(include_scissor_first_vert_ >= 0);
  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, include_scissor_first_vert_, 4));
  graphics_state->DisableDepthWrite();

  // Set the depth test function for subsequent draws to occur within the
  // stencil. Be sure to call UndoStencilState() when drawing in the
  // stencilled area is no longer desired.
  GL_CALL(glDepthFunc(GL_EQUAL));
}

void DrawDepthStencil::AddStencil(
    const math::RectF& include_scissor,
    const math::RectF& exclude_scissor) {
  // At least the include_scissor must be specified.
  SB_DCHECK(!include_scissor.IsEmpty());

  include_scissor_first_vert_ = static_cast<GLint>(attributes_.size());
  AddRect(include_scissor, 0);
  if (!exclude_scissor.IsEmpty()) {
    // Exclude scissor must use the next closest depth.
    float depth = base_state_.depth;
    base_state_.depth = GraphicsState::NextClosestDepth(depth);
    exclude_scissor_first_vert_ = static_cast<GLint>(attributes_.size());
    AddRect(exclude_scissor, 0);
    base_state_.depth = depth;
  }
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
