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
    : DrawObject(base_state) {
  uint32_t color32 = GetGLRGBA(color * base_state_.opacity);
  attributes_.reserve(4);
  AddVertex(rect.x(), rect.y(), color32);
  AddVertex(rect.x(), rect.bottom(), color32);
  AddVertex(rect.right(), rect.y(), color32);
  AddVertex(rect.right(), rect.bottom(), color32);
  graphics_state->ReserveVertexData(attributes_.size() *
                                    sizeof(VertexAttributes));
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
      program->GetVertexShader().a_position(), 3, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, position));
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_color(), 4, GL_UNSIGNED_BYTE, GL_TRUE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, color));
  graphics_state->VertexAttribFinish();
  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, attributes_.size()));
}

void DrawPolyColor::AddVertex(float x, float y, uint32_t color) {
  VertexAttributes attribute = { { x, y, base_state_.depth }, color };
  attributes_.push_back(attribute);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
