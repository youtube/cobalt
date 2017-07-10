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
#include "egl/generated_shader_impl.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
const int kVertexCount = 10;
}

DrawRectShadowSpread::DrawRectShadowSpread(GraphicsState* graphics_state,
    const BaseState& base_state, const math::RectF& inner_rect,
    const math::RectF& outer_rect, const render_tree::ColorRGBA& color,
    const math::RectF& include_scissor)
    : DrawObject(base_state),
      inner_rect_(inner_rect),
      outer_rect_(outer_rect),
      include_scissor_(include_scissor),
      vertex_buffer_(nullptr) {
  color_ = GetGLRGBA(color * base_state_.opacity);
  graphics_state->ReserveVertexData(kVertexCount * sizeof(VertexAttributes));
}

void DrawRectShadowSpread::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  // Draw the box shadow's spread. This is a triangle strip covering the area
  // between outer rect and inner rect.
  VertexAttributes attributes[kVertexCount];
  SetVertex(&attributes[0], outer_rect_.x(), outer_rect_.y());
  SetVertex(&attributes[1], inner_rect_.x(), inner_rect_.y());
  SetVertex(&attributes[2], outer_rect_.right(), outer_rect_.y());
  SetVertex(&attributes[3], inner_rect_.right(), inner_rect_.y());
  SetVertex(&attributes[4], outer_rect_.right(), outer_rect_.bottom());
  SetVertex(&attributes[5], inner_rect_.right(), inner_rect_.bottom());
  SetVertex(&attributes[6], outer_rect_.x(), outer_rect_.bottom());
  SetVertex(&attributes[7], inner_rect_.x(), inner_rect_.bottom());
  SetVertex(&attributes[8], outer_rect_.x(), outer_rect_.y());
  SetVertex(&attributes[9], inner_rect_.x(), inner_rect_.y());

  vertex_buffer_ = graphics_state->AllocateVertexData(sizeof(attributes));
  SbMemoryCopy(vertex_buffer_, attributes, sizeof(attributes));
}

void DrawRectShadowSpread::ExecuteRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  ShaderProgram<ShaderVertexColorOffset,
                ShaderFragmentColorInclude>* program;
  program_manager->GetProgram(&program);
  graphics_state->UseProgram(program->GetHandle());
  graphics_state->UpdateClipAdjustment(
      program->GetVertexShader().u_clip_adjustment());
  graphics_state->UpdateTransformMatrix(
      program->GetVertexShader().u_view_matrix(),
      base_state_.transform);
  graphics_state->Scissor(base_state_.scissor.x(), base_state_.scissor.y(),
      base_state_.scissor.width(), base_state_.scissor.height());
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_position(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, position));
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_color(), 4, GL_UNSIGNED_BYTE, GL_TRUE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, color));
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_offset(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, offset));
  graphics_state->VertexAttribFinish();

  float include[4] = {
    include_scissor_.x(),
    include_scissor_.y(),
    include_scissor_.right(),
    include_scissor_.bottom()
  };
  GL_CALL(glUniform4fv(program->GetFragmentShader().u_include(), 1, include));

  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, kVertexCount));
}

base::TypeId DrawRectShadowSpread::GetTypeId() const {
  return ShaderProgram<ShaderVertexColorOffset,
                       ShaderFragmentColorInclude>::GetTypeId();
}

void DrawRectShadowSpread::SetVertex(VertexAttributes* vertex,
                                     float x, float y) {
  vertex->position[0] = x;
  vertex->position[1] = y;
  vertex->offset[0] = x;
  vertex->offset[1] = y;
  vertex->color = color_;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
