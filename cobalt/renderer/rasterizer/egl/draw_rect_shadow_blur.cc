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

#include "cobalt/renderer/rasterizer/egl/draw_rect_shadow_blur.h"

#include <GLES2/gl2.h>
#include <algorithm>

#include "cobalt/renderer/backend/egl/utils.h"
#include "egl/generated_shader_impl.h"
#include "starboard/log.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

DrawRectShadowBlur::DrawRectShadowBlur(GraphicsState* graphics_state,
    const BaseState& base_state, const math::RectF& inner_rect,
    const math::RectF& outer_rect, const math::RectF& blur_edge,
    const render_tree::ColorRGBA& color, const math::RectF& exclude_scissor,
    float blur_sigma, bool inset)
    : DrawObject(base_state),
      draw_stencil_(graphics_state, base_state, outer_rect, exclude_scissor),
      blur_center_(blur_edge.CenterPoint()),
      // The sigma scale is used to transform pixel distances to sigma-relative
      // distances. The 0.5 multiplier is used to match skia's implementation.
      blur_sigma_scale_(0.5f / blur_sigma),
      vertex_buffer_(NULL) {
  float color_scale = 1.0f;
  attributes_.reserve(10);

  // The blur radius dictates the distance from blur center at which the
  // shadow should be about 50% opacity of the shadow color. This is expressed
  // in terms of sigma.
  blur_radius_[0] = blur_edge.width() * 0.5f * blur_sigma_scale_;
  blur_radius_[1] = blur_edge.height() * 0.5f * blur_sigma_scale_;

  if (inset) {
    // Invert the shadow by using (1.0 - weight) for the alpha scale.
    blur_scale_add_[0] = -1.0f;
    blur_scale_add_[1] = 1.0f;
  } else {
    // The calculated weight works for outset shadows.
    blur_scale_add_[0] = 1.0f;
    blur_scale_add_[1] = 0.0f;

    // Scale opacity down if the shadow's edge is smaller than the blur kernel.
    // Use a quick linear scale as the dimension goes below 1.5 sigma (which is
    // the distance at which the approximated gaussian integral reaches 0).
    const float size_scale = 1.0f / (1.5f * blur_sigma);
    color_scale = std::min(blur_edge.width() * size_scale, 1.0f) *
                  std::min(blur_edge.height() * size_scale, 1.0f);
  }

  // Add a triangle-strip to draw the area between outer_rect and inner_rect.
  // This is the area which the shadow covers.
  uint32_t color32 = GetGLRGBA(color * color_scale * base_state_.opacity);
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

  graphics_state->ReserveVertexData(
      attributes_.size() * sizeof(VertexAttributes));
}

void DrawRectShadowBlur::ExecuteOnscreenUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  draw_stencil_.ExecuteOnscreenUpdateVertexBuffer(
      graphics_state, program_manager);
  vertex_buffer_ = graphics_state->AllocateVertexData(
      attributes_.size() * sizeof(VertexAttributes));
  SbMemoryCopy(vertex_buffer_, &attributes_[0],
               attributes_.size() * sizeof(VertexAttributes));
}

void DrawRectShadowBlur::ExecuteOnscreenRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  // Create a stencil for the pixels to be modified.
  draw_stencil_.ExecuteOnscreenRasterize(
      graphics_state, program_manager);

  // Draw the blurred shadow in the stencilled area.
  ShaderProgram<ShaderVertexColorBlur,
                ShaderFragmentColorBlur>* program;
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
  graphics_state->VertexAttribPointer(
      program->GetVertexShader().a_blur_position(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, blur_position));
  graphics_state->VertexAttribFinish();
  GL_CALL(glUniform2fv(program->GetFragmentShader().u_blur_radius(), 1,
      blur_radius_));
  GL_CALL(glUniform2fv(program->GetFragmentShader().u_scale_add(), 1,
      blur_scale_add_));

  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, attributes_.size()));
  draw_stencil_.UndoStencilState(graphics_state);
}

void DrawRectShadowBlur::AddVertex(float x, float y, uint32_t color) {
  float blur_x = (x - blur_center_.x()) * blur_sigma_scale_;
  float blur_y = (y - blur_center_.y()) * blur_sigma_scale_;
  VertexAttributes attribute = { { x, y, base_state_.depth }, color,
                                 { blur_x, blur_y } };
  attributes_.push_back(attribute);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
