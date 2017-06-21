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

#include "cobalt/renderer/rasterizer/egl/draw_poly_color.h"

#include <GLES2/gl2.h>

#include "cobalt/renderer/backend/egl/utils.h"
#include "egl/generated_shader_impl.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

DrawPolyColor::DrawPolyColor(GraphicsState* graphics_state,
    const BaseState& base_state, const math::RectF& rect,
    const render_tree::ColorRGBA& color)
    : DrawObject(base_state),
      vertex_buffer_(NULL) {
  attributes_.reserve(4);
  AddRect(rect, GetGLRGBA(color * base_state_.opacity));
  graphics_state->ReserveVertexData(
      attributes_.size() * sizeof(VertexAttributes));
}

DrawPolyColor::DrawPolyColor(const BaseState& base_state)
    : DrawObject(base_state),
      vertex_buffer_(NULL) {
}

void DrawPolyColor::ExecuteOnscreenUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  vertex_buffer_ = graphics_state->AllocateVertexData(
      attributes_.size() * sizeof(VertexAttributes));
  SbMemoryCopy(vertex_buffer_, &attributes_[0],
               attributes_.size() * sizeof(VertexAttributes));
}

void DrawPolyColor::ExecuteOnscreenRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  SetupShader(graphics_state, program_manager);
  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, attributes_.size()));
}

void DrawPolyColor::SetupShader(GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  ShaderProgram<ShaderVertexColor,
                ShaderFragmentColor>* program;
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
  graphics_state->VertexAttribFinish();
}

void DrawPolyColor::AddRect(const math::RectF& rect, uint32_t color) {
  AddVertex(rect.x(), rect.y(), color);
  AddVertex(rect.x(), rect.bottom(), color);
  AddVertex(rect.right(), rect.y(), color);
  AddVertex(rect.right(), rect.bottom(), color);
}

void DrawPolyColor::AddVertex(float x, float y, uint32_t color) {
  VertexAttributes attribute = { { x, y }, color };
  attributes_.push_back(attribute);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
