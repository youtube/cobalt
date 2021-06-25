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

#include "cobalt/renderer/rasterizer/egl/draw_rect_shadow_spread.h"

#include <algorithm>

#include "base/logging.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

DrawRectShadowSpread::VertexAttributesSquare::VertexAttributesSquare(
    float x, float y, uint32_t in_color) {
  position[0] = x;
  position[1] = y;
  color = in_color;
}

DrawRectShadowSpread::VertexAttributesRound::VertexAttributesRound(
    float x, float y, const RCorner& inner, const RCorner& outer) {
  position[0] = x;
  position[1] = y;
  rcorner_inner = RCorner(position, inner);
  rcorner_outer = RCorner(position, outer);
}

DrawRectShadowSpread::DrawRectShadowSpread(
    GraphicsState* graphics_state, const BaseState& base_state,
    const math::RectF& inner_rect, const OptionalRoundedCorners& inner_corners,
    const math::RectF& outer_rect, const OptionalRoundedCorners& outer_corners,
    const render_tree::ColorRGBA& color)
    : DrawObject(base_state), vertex_buffer_(nullptr), index_buffer_(nullptr) {
  color_ = GetDrawColor(color) * base_state_.opacity;

  // Extract scale from the transform and move it into the vertex attributes
  // so that the anti-aliased edges remain 1 pixel wide.
  math::Vector2dF scale = RemoveScaleFromTransform();
  math::RectF inside_rect(inner_rect);
  math::RectF outside_rect(outer_rect);
  inside_rect.Scale(scale.x(), scale.y());
  outside_rect.Scale(scale.x(), scale.y());

  if (inner_corners || outer_corners) {
    // If using rounded corners, then both inner and outer rects must have
    // rounded corner definitions.
    DCHECK(inner_corners);
    DCHECK(outer_corners);
    render_tree::RoundedCorners inside_corners =
        inner_corners->Scale(scale.x(), scale.y());
    render_tree::RoundedCorners outside_corners =
        outer_corners->Scale(scale.x(), scale.y());
    SetGeometry(graphics_state, inside_rect, inside_corners, outside_rect,
                outside_corners);
  } else {
    SetGeometry(graphics_state, inside_rect, outside_rect);
  }
}

void DrawRectShadowSpread::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  if (attributes_square_.size() > 0) {
    vertex_buffer_ = graphics_state->AllocateVertexData(
        attributes_square_.size() * sizeof(attributes_square_[0]));
    memcpy(vertex_buffer_, &attributes_square_[0],
                 attributes_square_.size() * sizeof(attributes_square_[0]));
  } else if (attributes_round_.size() > 0) {
    vertex_buffer_ = graphics_state->AllocateVertexData(
        attributes_round_.size() * sizeof(attributes_round_[0]));
    memcpy(vertex_buffer_, &attributes_round_[0],
                 attributes_round_.size() * sizeof(attributes_round_[0]));
    index_buffer_ = graphics_state->AllocateVertexIndices(indices_.size());
    memcpy(index_buffer_, &indices_[0],
                 indices_.size() * sizeof(indices_[0]));
  }
}

void DrawRectShadowSpread::ExecuteRasterize(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  if (vertex_buffer_ == nullptr) {
    return;
  }

  // Draw the box shadow.
  if (attributes_square_.size() > 0) {
    ShaderProgram<ShaderVertexColor, ShaderFragmentColor>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupVertexShader(graphics_state, program->GetVertexShader());
    GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, attributes_square_.size()));
  } else {
    ShaderProgram<ShaderVertexRcorner2, ShaderFragmentRcorner2Color>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupVertexShader(graphics_state, program->GetVertexShader());
    GL_CALL(glUniform4f(program->GetFragmentShader().u_color(), color_.r(),
                        color_.g(), color_.b(), color_.a()));
    GL_CALL(
        glDrawElements(GL_TRIANGLES, indices_.size(), GL_UNSIGNED_SHORT,
                       graphics_state->GetVertexIndexPointer(index_buffer_)));
  }
}

base::TypeId DrawRectShadowSpread::GetTypeId() const {
  if (attributes_square_.size() > 0) {
    return ShaderProgram<ShaderVertexColor, ShaderFragmentColor>::GetTypeId();
  } else {
    return ShaderProgram<ShaderVertexRcorner2,
                         ShaderFragmentRcorner2Color>::GetTypeId();
  }
}

void DrawRectShadowSpread::SetupVertexShader(GraphicsState* graphics_state,
                                             const ShaderVertexColor& shader) {
  graphics_state->UpdateClipAdjustment(shader.u_clip_adjustment());
  graphics_state->UpdateTransformMatrix(shader.u_view_matrix(),
                                        base_state_.transform);
  graphics_state->Scissor(base_state_.scissor.x(), base_state_.scissor.y(),
                          base_state_.scissor.width(),
                          base_state_.scissor.height());
  graphics_state->VertexAttribPointer(
      shader.a_position(), 2, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributesSquare),
      vertex_buffer_ + offsetof(VertexAttributesSquare, position));
  graphics_state->VertexAttribPointer(
      shader.a_color(), 4, GL_UNSIGNED_BYTE, GL_TRUE,
      sizeof(VertexAttributesSquare),
      vertex_buffer_ + offsetof(VertexAttributesSquare, color));
  graphics_state->VertexAttribFinish();
}

void DrawRectShadowSpread::SetupVertexShader(
    GraphicsState* graphics_state, const ShaderVertexRcorner2& shader) {
  graphics_state->UpdateClipAdjustment(shader.u_clip_adjustment());
  graphics_state->UpdateTransformMatrix(shader.u_view_matrix(),
                                        base_state_.transform);
  graphics_state->Scissor(base_state_.scissor.x(), base_state_.scissor.y(),
                          base_state_.scissor.width(),
                          base_state_.scissor.height());
  graphics_state->VertexAttribPointer(
      shader.a_position(), 2, GL_FLOAT, GL_FALSE, sizeof(VertexAttributesRound),
      vertex_buffer_ + offsetof(VertexAttributesRound, position));
  graphics_state->VertexAttribPointer(
      shader.a_rcorner_inner(), 4, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributesRound),
      vertex_buffer_ + offsetof(VertexAttributesRound, rcorner_inner));
  graphics_state->VertexAttribPointer(
      shader.a_rcorner_outer(), 4, GL_FLOAT, GL_FALSE,
      sizeof(VertexAttributesRound),
      vertex_buffer_ + offsetof(VertexAttributesRound, rcorner_outer));
  graphics_state->VertexAttribFinish();
}

void DrawRectShadowSpread::SetGeometry(GraphicsState* graphics_state,
                                       const math::RectF& inner_rect,
                                       const math::RectF& outer_rect) {
  // Draw the box shadow's spread. This is a triangle strip covering the area
  // between outer rect and inner rect.
  uint32_t color = GetGLRGBA(color_);

  if (inner_rect.IsEmpty()) {
    attributes_square_.reserve(4);
    attributes_square_.emplace_back(outer_rect.x(), outer_rect.y(), color);
    attributes_square_.emplace_back(outer_rect.right(), outer_rect.y(), color);
    attributes_square_.emplace_back(outer_rect.x(), outer_rect.bottom(), color);
    attributes_square_.emplace_back(outer_rect.right(), outer_rect.bottom(),
                                    color);
  } else {
    math::RectF inside_rect(inner_rect);
    inside_rect.Intersect(outer_rect);
    attributes_square_.reserve(10);
    attributes_square_.emplace_back(outer_rect.x(), outer_rect.y(), color);
    attributes_square_.emplace_back(inside_rect.x(), inside_rect.y(), color);
    attributes_square_.emplace_back(outer_rect.right(), outer_rect.y(), color);
    attributes_square_.emplace_back(inside_rect.right(), inside_rect.y(),
                                    color);
    attributes_square_.emplace_back(outer_rect.right(), outer_rect.bottom(),
                                    color);
    attributes_square_.emplace_back(inside_rect.right(), inside_rect.bottom(),
                                    color);
    attributes_square_.emplace_back(outer_rect.x(), outer_rect.bottom(), color);
    attributes_square_.emplace_back(inside_rect.x(), inside_rect.bottom(),
                                    color);
    attributes_square_.emplace_back(outer_rect.x(), outer_rect.y(), color);
    attributes_square_.emplace_back(inside_rect.x(), inside_rect.y(), color);
  }

  graphics_state->ReserveVertexData(attributes_square_.size() *
                                    sizeof(attributes_square_[0]));
}

void DrawRectShadowSpread::SetGeometry(
    GraphicsState* graphics_state, const math::RectF& inner_rect,
    const render_tree::RoundedCorners& inner_corners,
    const math::RectF& outer_rect,
    const render_tree::RoundedCorners& outer_corners) {
  // Draw the area between the inner rounded rect and outer rounded rect. Add
  // a 1-pixel border to include antialiasing.
  math::RectF bounds(outer_rect);
  bounds.Outset(1.0f, 1.0f);

  // Get the render quads for the inner rounded rect excluding its inscribed
  // rectangle.
  RRectAttributes rrect_inner[8];
  GetRRectAttributes(bounds, inner_rect, inner_corners, rrect_inner);

  // Get the render quads for the outer rounded rect.
  RRectAttributes rrect_outer[4];
  GetRRectAttributes(bounds, outer_rect, outer_corners, rrect_outer);

  // Add geometry to draw the area between the inner rrect and outer rrect.
  for (int i = 0; i < arraysize(rrect_inner); ++i) {
    for (int o = 0; o < arraysize(rrect_outer); ++o) {
      math::RectF intersection =
          math::IntersectRects(rrect_inner[i].bounds, rrect_outer[o].bounds);
      if (!intersection.IsEmpty()) {
        // Use two triangles to draw the intersection.
        const RCorner& inner = rrect_inner[i].rcorner;
        const RCorner& outer = rrect_outer[o].rcorner;
        uint16_t vert = static_cast<uint16_t>(attributes_round_.size());
        attributes_round_.emplace_back(intersection.x(), intersection.y(),
                                       inner, outer);
        attributes_round_.emplace_back(intersection.right(), intersection.y(),
                                       inner, outer);
        attributes_round_.emplace_back(intersection.x(), intersection.bottom(),
                                       inner, outer);
        attributes_round_.emplace_back(intersection.right(),
                                       intersection.bottom(), inner, outer);
        indices_.emplace_back(vert);
        indices_.emplace_back(vert + 1);
        indices_.emplace_back(vert + 2);
        indices_.emplace_back(vert + 1);
        indices_.emplace_back(vert + 2);
        indices_.emplace_back(vert + 3);
      }
    }
  }

  graphics_state->ReserveVertexData(attributes_round_.size() *
                                    sizeof(attributes_round_[0]));
  graphics_state->ReserveVertexIndices(indices_.size());
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
