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

#include "base/logging.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
const int kVertexCount = 10;
const int kVertexCountRRect = 4;
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
      vertex_buffer_(nullptr) {
  color_ = GetGLRGBA(color * base_state_.opacity);
  if (inner_corners_ || outer_corners_) {
    // If using rounded corners, then both inner and outer rects must have
    // rounded corner definitions.
    DCHECK(inner_corners_);
    DCHECK(outer_corners_);
    graphics_state->ReserveVertexData(
        kVertexCountRRect * sizeof(VertexAttributes));
  } else {
    // Only pixels inside the outer rect should be touched.
    inner_rect_.Intersect(outer_rect_);
    graphics_state->ReserveVertexData(kVertexCount * sizeof(VertexAttributes));
  }
}

void DrawRectShadowSpread::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  if (inner_corners_) {
    // Draw a rect encompassing the outer rect with a 1 pixel border for
    // anti-aliasing, and let the shader decide which pixels to modify.
    math::RectF rect(outer_rect_);
    rect.Outset(1.0f, 1.0f);
    VertexAttributes attributes[kVertexCountRRect];
    SetVertex(&attributes[0], rect.x(), rect.y());
    SetVertex(&attributes[1], rect.right(), rect.y());
    SetVertex(&attributes[2], rect.x(), rect.bottom());
    SetVertex(&attributes[3], rect.right(), rect.bottom());
    vertex_buffer_ = graphics_state->AllocateVertexData(sizeof(attributes));
    SbMemoryCopy(vertex_buffer_, attributes, sizeof(attributes));
  } else {
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
}

void DrawRectShadowSpread::ExecuteRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  if (inner_corners_) {
    ShaderProgram<VertexShader, ShaderFragmentColorBetweenRrects>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupShader(program->GetVertexShader(), graphics_state);

    SetRRectUniforms(program->GetFragmentShader().u_inner_rect(),
                     program->GetFragmentShader().u_inner_corners(),
                     inner_rect_, *inner_corners_);
    SetRRectUniforms(program->GetFragmentShader().u_outer_rect(),
                     program->GetFragmentShader().u_outer_corners(),
                     outer_rect_, *outer_corners_);
    GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, kVertexCountRRect));
  } else {
    ShaderProgram<VertexShader, ShaderFragmentColorInclude>* program;
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
    GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, kVertexCount));
  }
}

void DrawRectShadowSpread::SetupShader(const VertexShader& shader,
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
    return ShaderProgram<VertexShader,
                         ShaderFragmentColorBetweenRrects>::GetTypeId();
  } else {
    return ShaderProgram<VertexShader,
                         ShaderFragmentColorInclude>::GetTypeId();
  }
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
