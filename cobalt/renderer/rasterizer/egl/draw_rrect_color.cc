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

#include "cobalt/renderer/rasterizer/egl/draw_rrect_color.h"

#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "egl/generated_shader_impl.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
const int kVertexCount = 4 * 6;
}  // namespace

DrawRRectColor::DrawRRectColor(GraphicsState* graphics_state,
                               const BaseState& base_state,
                               const math::RectF& rect,
                               const render_tree::RoundedCorners& corners,
                               const render_tree::ColorRGBA& color)
    : DrawObject(base_state),
      rect_(rect),
      corners_(corners),
      vertex_buffer_(NULL) {
  color_ = GetDrawColor(color) * base_state_.opacity;
  graphics_state->ReserveVertexData(kVertexCount * sizeof(VertexAttributes));

  // Extract scale from the transform and move it into the vertex attributes
  // so that the anti-aliased edges remain 1 pixel wide.
  math::Vector2dF scale = RemoveScaleFromTransform();
  rect_.Scale(scale.x(), scale.y());
  corners_ = corners_.Scale(scale.x(), scale.y());
}

void DrawRRectColor::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  // Draw the rect with an extra pixel outset for anti-aliasing. The fragment
  // shader will handle the rounded corners.
  VertexAttributes attributes[kVertexCount];
  math::RectF outer_rect(rect_);
  outer_rect.Outset(1.0f, 1.0f);

  RRectAttributes rrect[4];
  GetRRectAttributes(outer_rect, rect_, corners_, rrect);
  for (int r = 0, v = 0; r < arraysize(rrect); ++r) {
    attributes[v].position[0] = rrect[r].bounds.x();
    attributes[v].position[1] = rrect[r].bounds.y();
    attributes[v].rcorner = RCorner(attributes[v].position, rrect[r].rcorner);
    attributes[v + 1].position[0] = rrect[r].bounds.right();
    attributes[v + 1].position[1] = rrect[r].bounds.y();
    attributes[v + 1].rcorner =
        RCorner(attributes[v + 1].position, rrect[r].rcorner);
    attributes[v + 2].position[0] = rrect[r].bounds.x();
    attributes[v + 2].position[1] = rrect[r].bounds.bottom();
    attributes[v + 2].rcorner =
        RCorner(attributes[v + 2].position, rrect[r].rcorner);
    attributes[v + 3].position[0] = rrect[r].bounds.right();
    attributes[v + 3].position[1] = rrect[r].bounds.bottom();
    attributes[v + 3].rcorner =
        RCorner(attributes[v + 3].position, rrect[r].rcorner);
    attributes[v + 4] = attributes[v + 1];
    attributes[v + 5] = attributes[v + 2];
    v += 6;
  }

  vertex_buffer_ = graphics_state->AllocateVertexData(sizeof(attributes));
  memcpy(vertex_buffer_, attributes, sizeof(attributes));
}

void DrawRRectColor::ExecuteRasterize(GraphicsState* graphics_state,
                                      ShaderProgramManager* program_manager) {
  ShaderProgram<ShaderVertexRcorner, ShaderFragmentRcornerColor>* program;
  program_manager->GetProgram(&program);
  graphics_state->UseProgram(program->GetHandle());
  graphics_state->UpdateClipAdjustment(
      program->GetVertexShader().u_clip_adjustment());
  graphics_state->UpdateTransformMatrix(
      program->GetVertexShader().u_view_matrix(), base_state_.transform);
  graphics_state->Scissor(base_state_.scissor.x(), base_state_.scissor.y(),
                          base_state_.scissor.width(),
                          base_state_.scissor.height());
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_position(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes),
      vertex_buffer_ + offsetof(VertexAttributes, position));
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_rcorner(), 4, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes),
      vertex_buffer_ + offsetof(VertexAttributes, rcorner));
  graphics_state->VertexAttribFinish();

  GL_CALL(glUniform4f(program->GetFragmentShader().u_color(), color_.r(),
                      color_.g(), color_.b(), color_.a()));
  GL_CALL(glDrawArrays(GL_TRIANGLES, 0, kVertexCount));
}

base::TypeId DrawRRectColor::GetTypeId() const {
  return ShaderProgram<ShaderVertexRcorner,
                       ShaderFragmentRcornerColor>::GetTypeId();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
