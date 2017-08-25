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
#include <algorithm>

#include "base/logging.h"
#include "cobalt/renderer/backend/egl/utils.h"
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
    const OptionalRoundedCorners& inner_corners, const math::RectF& outer_rect,
    const OptionalRoundedCorners& outer_corners,
    const render_tree::ColorRGBA& color)
    : DrawObject(base_state),
      inner_rect_(inner_rect),
      outer_rect_(outer_rect),
      inner_corners_(inner_corners),
      outer_corners_(outer_corners),
      offset_scale_(1.0f),
      vertex_buffer_(nullptr),
      vertex_count_(0) {
  color_ = GetGLRGBA(GetDrawColor(color) * base_state_.opacity);
  if (inner_corners_ || outer_corners_) {
    // If using rounded corners, then both inner and outer rects must have
    // rounded corner definitions.
    DCHECK(inner_corners_);
    DCHECK(outer_corners_);
  }
  graphics_state->ReserveVertexData(kVertexCount * sizeof(VertexAttributes));
}

DrawRectShadowSpread::DrawRectShadowSpread(GraphicsState* graphics_state,
    const BaseState& base_state)
    : DrawObject(base_state),
      offset_scale_(1.0f),
      vertex_buffer_(nullptr),
      vertex_count_(0) {
  graphics_state->ReserveVertexData(kVertexCount * sizeof(VertexAttributes));
}

void DrawRectShadowSpread::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  // Draw the box shadow's spread. This is a triangle strip covering the area
  // between outer rect and inner rect.
  math::RectF inside_rect(inner_rect_);
  math::RectF outside_rect(outer_rect_);
  VertexAttributes attributes[kVertexCount];

  if (inner_corners_) {
    // Inset the inside rect to include the rounded corners.
    inside_rect.Inset(
        std::max(inner_corners_->bottom_left.horizontal,
                 inner_corners_->top_left.horizontal),
        std::max(inner_corners_->top_left.vertical,
                 inner_corners_->top_right.vertical),
        std::max(inner_corners_->top_right.horizontal,
                 inner_corners_->bottom_right.horizontal),
        std::max(inner_corners_->bottom_right.vertical,
                 inner_corners_->bottom_left.vertical));

    // Add a 1 pixel border to the outer rect for anti-aliasing.
    outside_rect.Outset(1.0f, 1.0f);
  }

  // Only pixels inside the outer rect should be touched.
  if (inside_rect.IsEmpty()) {
    vertex_count_ = 4;
    SetVertex(&attributes[0], outside_rect.x(), outside_rect.y());
    SetVertex(&attributes[1], outside_rect.right(), outside_rect.y());
    SetVertex(&attributes[2], outside_rect.x(), outside_rect.bottom());
    SetVertex(&attributes[3], outside_rect.right(), outside_rect.bottom());
  } else {
    inside_rect.Intersect(outside_rect);
    vertex_count_ = 10;
    SetVertex(&attributes[0], outside_rect.x(), outside_rect.y());
    SetVertex(&attributes[1], inside_rect.x(), inside_rect.y());
    SetVertex(&attributes[2], outside_rect.right(), outside_rect.y());
    SetVertex(&attributes[3], inside_rect.right(), inside_rect.y());
    SetVertex(&attributes[4], outside_rect.right(), outside_rect.bottom());
    SetVertex(&attributes[5], inside_rect.right(), inside_rect.bottom());
    SetVertex(&attributes[6], outside_rect.x(), outside_rect.bottom());
    SetVertex(&attributes[7], inside_rect.x(), inside_rect.bottom());
    SetVertex(&attributes[8], outside_rect.x(), outside_rect.y());
    SetVertex(&attributes[9], inside_rect.x(), inside_rect.y());
  }

  vertex_buffer_ = graphics_state->AllocateVertexData(
      vertex_count_ * sizeof(VertexAttributes));
  SbMemoryCopy(vertex_buffer_, attributes,
      vertex_count_ * sizeof(VertexAttributes));
}

void DrawRectShadowSpread::ExecuteRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  if (inner_corners_) {
    ShaderProgram<CommonVertexShader,
                  ShaderFragmentColorBetweenRrects>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupShader(program->GetVertexShader(), graphics_state);

    SetRRectUniforms(program->GetFragmentShader().u_inner_rect(),
                     program->GetFragmentShader().u_inner_corners(),
                     inner_rect_, *inner_corners_, 0.5f);
    SetRRectUniforms(program->GetFragmentShader().u_outer_rect(),
                     program->GetFragmentShader().u_outer_corners(),
                     outer_rect_, *outer_corners_, 0.5f);
  } else {
    ShaderProgram<CommonVertexShader, ShaderFragmentColorInclude>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupShader(program->GetVertexShader(), graphics_state);

    float include[4] = {
      outer_rect_.x(),
      outer_rect_.y(),
      outer_rect_.right(),
      outer_rect_.bottom()
    };
    GL_CALL(glUniform4fv(program->GetFragmentShader().u_include(), 1, include));
  }

  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, vertex_count_));
}

void DrawRectShadowSpread::SetupShader(const CommonVertexShader& shader,
    GraphicsState* graphics_state) {
  graphics_state->UpdateClipAdjustment(shader.u_clip_adjustment());
  graphics_state->UpdateTransformMatrix(shader.u_view_matrix(),
      base_state_.transform);
  graphics_state->Scissor(base_state_.scissor.x(), base_state_.scissor.y(),
      base_state_.scissor.width(), base_state_.scissor.height());
  graphics_state->VertexAttribPointer(
      shader.a_position(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, position));
  graphics_state->VertexAttribPointer(
      shader.a_color(), 4, GL_UNSIGNED_BYTE, GL_TRUE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, color));
  graphics_state->VertexAttribPointer(
      shader.a_offset(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributes), vertex_buffer_ +
      offsetof(VertexAttributes, offset));
  graphics_state->VertexAttribFinish();
}

base::TypeId DrawRectShadowSpread::GetTypeId() const {
  if (inner_corners_) {
    return ShaderProgram<CommonVertexShader,
                         ShaderFragmentColorBetweenRrects>::GetTypeId();
  } else {
    return ShaderProgram<CommonVertexShader,
                         ShaderFragmentColorInclude>::GetTypeId();
  }
}

void DrawRectShadowSpread::SetVertex(VertexAttributes* vertex,
                                     float x, float y) {
  vertex->position[0] = x;
  vertex->position[1] = y;
  vertex->offset[0] = (x - offset_center_.x()) * offset_scale_;
  vertex->offset[1] = (y - offset_center_.y()) * offset_scale_;
  vertex->color = color_;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
