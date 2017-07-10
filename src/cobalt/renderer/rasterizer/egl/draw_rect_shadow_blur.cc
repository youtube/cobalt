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

namespace {
const int kVertexCount = 10;
}

DrawRectShadowBlur::DrawRectShadowBlur(GraphicsState* graphics_state,
    const BaseState& base_state, const math::RectF& inner_rect,
    const math::RectF& outer_rect, const math::RectF& blur_edge,
    const render_tree::ColorRGBA& color, float blur_sigma, bool inset)
    : DrawObject(base_state),
      inner_rect_(inner_rect),
      outer_rect_(outer_rect),
      blur_center_(blur_edge.CenterPoint()),
      // The sigma scale is used to transform pixel distances to sigma-relative
      // distances. The 0.5 multiplier is used to match skia's implementation.
      blur_sigma_scale_(0.5f / blur_sigma),
      vertex_buffer_(NULL) {
  float color_scale = 1.0f;

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

    // Ensure the outer_rect contains the inner_rect to avoid overlapping
    // triangles in the draw call. The fragment shader will properly fade out
    // the extra pixels.
    outer_rect_.Union(inner_rect_);
  }

  color_ = GetGLRGBA(color * color_scale * base_state_.opacity);
  graphics_state->ReserveVertexData(kVertexCount * sizeof(VertexAttributes));
}

void DrawRectShadowBlur::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  // Add a triangle-strip to draw the area between outer_rect and inner_rect.
  // This is the area which the shadow covers.
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

void DrawRectShadowBlur::ExecuteRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  // Draw the blurred shadow.
  ShaderProgram<ShaderVertexColorOffset,
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
  GL_CALL(glUniform2fv(program->GetFragmentShader().u_blur_radius(), 1,
      blur_radius_));
  GL_CALL(glUniform2fv(program->GetFragmentShader().u_scale_add(), 1,
      blur_scale_add_));

  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, kVertexCount));
}

base::TypeId DrawRectShadowBlur::GetTypeId() const {
  return ShaderProgram<ShaderVertexColorOffset,
                       ShaderFragmentColorBlur>::GetTypeId();
}

void DrawRectShadowBlur::SetVertex(VertexAttributes* vertex,
                                   float x, float y) {
  vertex->position[0] = x;
  vertex->position[1] = y;

  vertex->offset[0] = (x - blur_center_.x()) * blur_sigma_scale_;
  vertex->offset[1] = (y - blur_center_.y()) * blur_sigma_scale_;

  vertex->color = color_;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
