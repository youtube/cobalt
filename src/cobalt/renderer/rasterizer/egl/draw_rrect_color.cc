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

#include "cobalt/renderer/rasterizer/egl/draw_rrect_color.h"

#include <GLES2/gl2.h>

#include "cobalt/renderer/backend/egl/utils.h"
#include "egl/generated_shader_impl.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
const int kVertexCount = 4;

struct VertexAttributes {
  float position[2];
  float offset[2];
  uint32_t color;
};

void SetVertex(VertexAttributes* vertex, float x, float y, uint32_t color) {
  vertex->position[0] = x;
  vertex->position[1] = y;
  vertex->offset[0] = x;
  vertex->offset[1] = y;
  vertex->color = color;
}
}  // namespace

DrawRRectColor::DrawRRectColor(GraphicsState* graphics_state,
    const BaseState& base_state, const math::RectF& rect,
    const render_tree::RoundedCorners& corners,
    const render_tree::ColorRGBA& color)
    : DrawObject(base_state),
      rect_(rect),
      corners_(corners),
      vertex_buffer_(NULL) {
  color_ = GetGLRGBA(color * base_state_.opacity);
  corners_.Normalize(rect_);
  graphics_state->ReserveVertexData(kVertexCount * sizeof(VertexAttributes));
}

void DrawRRectColor::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  // Draw the rect with an extra pixel outset for anti-aliasing. The fragment
  // shader will handle the rounded corners.
  VertexAttributes attributes[kVertexCount];
  math::RectF outer_rect(rect_);
  outer_rect.Outset(1.0f, 1.0f);
  SetVertex(&attributes[0], outer_rect.x(), outer_rect.y(), color_);
  SetVertex(&attributes[1], outer_rect.right(), outer_rect.y(), color_);
  SetVertex(&attributes[2], outer_rect.right(), outer_rect.bottom(), color_);
  SetVertex(&attributes[3], outer_rect.x(), outer_rect.bottom(), color_);
  vertex_buffer_ = graphics_state->AllocateVertexData(sizeof(attributes));
  SbMemoryCopy(vertex_buffer_, attributes, sizeof(attributes));
}

void DrawRRectColor::ExecuteRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  ShaderProgram<ShaderVertexColorOffset,
                ShaderFragmentColorRrect>* program;
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
  SetRRectUniforms(program->GetFragmentShader().u_rect(),
                   program->GetFragmentShader().u_corners(),
                   rect_, corners_, 0.5f);
  GL_CALL(glDrawArrays(GL_TRIANGLE_FAN, 0, kVertexCount));
}

base::TypeId DrawRRectColor::GetTypeId() const {
  return ShaderProgram<ShaderVertexColorOffset,
                       ShaderFragmentColorRrect>::GetTypeId();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
