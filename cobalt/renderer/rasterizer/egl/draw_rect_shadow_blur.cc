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

#include "cobalt/renderer/rasterizer/egl/draw_rect_shadow_blur.h"

#include <algorithm>

#include "base/logging.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
const float kSqrt2 = 1.414213562f;
const float kSqrtPi = 1.772453851f;

// The blur kernel extends for a limited number of sigmas.
const float kBlurExtentInSigmas = 3.0f;

// The error function is used to calculate the gaussian blur. Inputs for
// the error function should be scaled by 1 / (sqrt(2) * sigma). Alternatively,
// it can be viewed as kBlurDistance is the input value at which the blur
// intensity is effectively 0.
const float kBlurDistance = kBlurExtentInSigmas / kSqrt2;

void SetBlurRRectUniforms(const ShaderFragmentColorBlurRrects& shader,
                          math::RectF rect, render_tree::RoundedCorners corners,
                          float sigma) {
  const float kBlurExtentInPixels = kBlurExtentInSigmas * sigma;
  const float kOffsetScale = kBlurDistance / kBlurExtentInPixels;

  // Ensure a minimum radius for each corner to avoid division by zero.
  // NOTE: The rounded rect is already specified in terms of sigma.
  const float kMinSize = 0.01f * kOffsetScale;
  rect.Outset(kMinSize, kMinSize);
  corners = corners.Inset(-kMinSize, -kMinSize, -kMinSize, -kMinSize);
  corners = corners.Normalize(rect);

  // A normalized rounded rect should have at least one Y value which the
  // corners do not cross.
  const float kCenterY =
      0.5f * (rect.y() +
              std::max(corners.top_left.vertical, corners.top_right.vertical)) +
      0.5f * (rect.bottom() - std::max(corners.bottom_left.vertical,
                                       corners.bottom_right.vertical));

  // The blur extent is (blur_size.y, rect_min.y, rect_max.y, rect_center.y).
  GL_CALL(glUniform4f(shader.u_blur_extent(),
                      kBlurExtentInPixels * kOffsetScale, rect.y(),
                      rect.bottom(), kCenterY));

  // The blur rounded rect is split into top and bottom halves.
  // The "start" values represent (left_start.xy, right_start.xy).
  // The "scale" values represent (left_radius.x, 1 / left_radius.y,
  //   right_radius.x, 1 / right_radius.y). The sign of the scale value helps
  //   to translate between position and corner offset values, where the corner
  //   offset is positive if the position is inside the rounded corner.
  GL_CALL(glUniform4f(shader.u_blur_start_top(),
                      rect.x() + corners.top_left.horizontal,
                      rect.y() + corners.top_left.vertical,
                      rect.right() - corners.top_right.horizontal,
                      rect.y() + corners.top_right.vertical));
  GL_CALL(glUniform4f(shader.u_blur_start_bottom(),
                      rect.x() + corners.bottom_left.horizontal,
                      rect.bottom() - corners.bottom_left.vertical,
                      rect.right() - corners.bottom_right.horizontal,
                      rect.bottom() - corners.bottom_right.vertical));
  GL_CALL(glUniform4f(shader.u_blur_scale_top(), -corners.top_left.horizontal,
                      -1.0f / corners.top_left.vertical,
                      corners.top_right.horizontal,
                      -1.0f / corners.top_right.vertical));
  GL_CALL(glUniform4f(
      shader.u_blur_scale_bottom(), -corners.bottom_left.horizontal,
      1.0f / corners.bottom_left.vertical, corners.bottom_right.horizontal,
      1.0f / corners.bottom_right.vertical));
}
}  // namespace

DrawRectShadowBlur::VertexAttributesSquare::VertexAttributesSquare(
    float x, float y, float offset_scale) {
  position[0] = x;
  position[1] = y;
  offset[0] = x * offset_scale;
  offset[1] = y * offset_scale;
}

DrawRectShadowBlur::VertexAttributesRound::VertexAttributesRound(
    float x, float y, float offset_scale, const RCorner& rcorner) {
  position[0] = x;
  position[1] = y;
  offset[0] = x * offset_scale;
  offset[1] = y * offset_scale;
  rcorner_scissor = RCorner(position, rcorner);
}

DrawRectShadowBlur::DrawRectShadowBlur(
    GraphicsState* graphics_state, const BaseState& base_state,
    const math::RectF& base_rect, const OptionalRoundedCorners& base_corners,
    const math::RectF& spread_rect,
    const OptionalRoundedCorners& spread_corners,
    const render_tree::ColorRGBA& color, float blur_sigma, bool inset)
    : DrawObject(base_state),
      spread_rect_(spread_rect),
      spread_corners_(spread_corners),
      blur_sigma_(blur_sigma),
      is_inset_(inset),
      vertex_buffer_(nullptr),
      index_buffer_(nullptr) {
  color_ = GetDrawColor(color) * base_state_.opacity;

  // Extract scale from the transform and move it into the vertex attributes
  // so that the anti-aliased edges remain 1 pixel wide.
  math::Vector2dF scale = RemoveScaleFromTransform();
  math::RectF scaled_base_rect(base_rect);
  scaled_base_rect.Scale(scale.x(), scale.y());
  OptionalRoundedCorners scaled_base_corners(base_corners);
  if (scaled_base_corners) {
    scaled_base_corners = scaled_base_corners->Scale(scale.x(), scale.y());
  }
  spread_rect_.Scale(scale.x(), scale.y());
  if (spread_corners_) {
    spread_corners_ = spread_corners_->Scale(scale.x(), scale.y());
  }

  // The blur algorithms used by the shaders do not produce good results with
  // separate x and y blur sigmas. Select a single blur sigma to approximate
  // the desired blur.
  blur_sigma_ *= std::sqrt(scale.x() * scale.y());

  SetGeometry(graphics_state, scaled_base_rect, scaled_base_corners);
}

void DrawRectShadowBlur::ExecuteUpdateVertexBuffer(
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

void DrawRectShadowBlur::ExecuteRasterize(
    GraphicsState* graphics_state, ShaderProgramManager* program_manager) {
  if (vertex_buffer_ == nullptr) {
    return;
  }

  // Draw the blurred shadow.
  if (spread_corners_) {
    ShaderProgram<ShaderVertexOffsetRcorner, ShaderFragmentColorBlurRrects>*
        program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupVertexShader(graphics_state, program->GetVertexShader());
    SetFragmentUniforms(program->GetFragmentShader().u_color(),
                        program->GetFragmentShader().u_scale_add());
    SetBlurRRectUniforms(program->GetFragmentShader(), spread_rect_,
                         *spread_corners_, blur_sigma_);
    GL_CALL(
        glDrawElements(GL_TRIANGLES, indices_.size(), GL_UNSIGNED_SHORT,
                       graphics_state->GetVertexIndexPointer(index_buffer_)));
  } else {
    ShaderProgram<ShaderVertexOffset, ShaderFragmentColorBlur>* program;
    program_manager->GetProgram(&program);
    graphics_state->UseProgram(program->GetHandle());
    SetupVertexShader(graphics_state, program->GetVertexShader());
    SetFragmentUniforms(program->GetFragmentShader().u_color(),
                        program->GetFragmentShader().u_scale_add());
    GL_CALL(glUniform4f(program->GetFragmentShader().u_blur_rect(),
                        spread_rect_.x(), spread_rect_.y(),
                        spread_rect_.right(), spread_rect_.bottom()));
    GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, attributes_square_.size()));
  }
}

base::TypeId DrawRectShadowBlur::GetTypeId() const {
  if (spread_corners_) {
    return ShaderProgram<ShaderVertexOffsetRcorner,
                         ShaderFragmentColorBlurRrects>::GetTypeId();
  } else {
    return ShaderProgram<ShaderVertexOffset,
                         ShaderFragmentColorBlur>::GetTypeId();
  }
}

void DrawRectShadowBlur::SetupVertexShader(GraphicsState* graphics_state,
                                           const ShaderVertexOffset& shader) {
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
      shader.a_offset(), 2, GL_FLOAT, GL_FALSE, sizeof(VertexAttributesSquare),
      vertex_buffer_ + offsetof(VertexAttributesSquare, offset));
  graphics_state->VertexAttribFinish();
}

void DrawRectShadowBlur::SetupVertexShader(
    GraphicsState* graphics_state, const ShaderVertexOffsetRcorner& shader) {
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
      shader.a_offset(), 2, GL_FLOAT, GL_FALSE, sizeof(VertexAttributesRound),
      vertex_buffer_ + offsetof(VertexAttributesRound, offset));
  graphics_state->VertexAttribPointer(
      shader.a_rcorner(), 4, GL_FLOAT, GL_FALSE, sizeof(VertexAttributesRound),
      vertex_buffer_ + offsetof(VertexAttributesRound, rcorner_scissor));
  graphics_state->VertexAttribFinish();
}

void DrawRectShadowBlur::SetFragmentUniforms(GLint color_uniform,
                                             GLint scale_add_uniform) {
  GL_CALL(glUniform4f(color_uniform, color_.r(), color_.g(), color_.b(),
                      color_.a()));
  if (is_inset_) {
    // Invert the shadow.
    GL_CALL(glUniform2f(scale_add_uniform, -1.0f, 1.0f));
  } else {
    // Keep the normal (outset) shadow.
    GL_CALL(glUniform2f(scale_add_uniform, 1.0f, 0.0f));
  }
}

void DrawRectShadowBlur::SetGeometry(
    GraphicsState* graphics_state, const math::RectF& base_rect,
    const OptionalRoundedCorners& base_corners) {
  const float kBlurExtentInPixels = kBlurExtentInSigmas * blur_sigma_;

  if (base_corners || spread_corners_) {
    // If rounded rects are specified, then both the base and spread rects
    // must have rounded corners.
    DCHECK(base_corners);
    DCHECK(spread_corners_);

    if (is_inset_) {
      // Extend the outer rect to include the antialiased edge.
      math::RectF outer_rect(base_rect);
      outer_rect.Outset(1.0f, 1.0f);
      RRectAttributes rrect_outer[4];
      GetRRectAttributes(outer_rect, base_rect, *base_corners, rrect_outer);
      // Inset the spread rect by the blur extent. Use that as the inner bounds.
      RRectAttributes rrect_inner[8];
      math::RectF inner_rect(spread_rect_);
      inner_rect.Inset(kBlurExtentInPixels, kBlurExtentInPixels);
      if (!inner_rect.IsEmpty()) {
        // Get the inner bounds excluding the inscribed rect.
        render_tree::RoundedCorners inner_corners =
            spread_corners_->Inset(kBlurExtentInPixels, kBlurExtentInPixels,
                                   kBlurExtentInPixels, kBlurExtentInPixels);
        inner_corners = inner_corners.Normalize(inner_rect);
        GetRRectAttributes(outer_rect, inner_rect, inner_corners, rrect_inner);
      } else {
        // The blur covers everything inside the outer rect.
        rrect_inner[0].bounds = outer_rect;
      }
      SetGeometry(graphics_state, rrect_outer, rrect_inner);
    } else {
      // Extend the outer rect to include the blur.
      math::RectF outer_rect(spread_rect_);
      outer_rect.Outset(kBlurExtentInPixels, kBlurExtentInPixels);
      // Exclude the inscribed rect of the base rounded rect.
      RRectAttributes rrect[8];
      GetRRectAttributes(outer_rect, base_rect, *base_corners, rrect);
      SetGeometry(graphics_state, rrect);
    }
  } else {
    // Handle box shadow with square corners.
    if (is_inset_) {
      math::RectF inner_rect(spread_rect_);
      inner_rect.Inset(kBlurExtentInPixels, kBlurExtentInPixels);
      SetGeometry(graphics_state, inner_rect, base_rect);
    } else {
      math::RectF outer_rect(spread_rect_);
      outer_rect.Outset(kBlurExtentInPixels, kBlurExtentInPixels);
      SetGeometry(graphics_state, base_rect, outer_rect);
    }
  }
}

void DrawRectShadowBlur::SetGeometry(GraphicsState* graphics_state,
                                     const math::RectF& inner_rect,
                                     const math::RectF& outer_rect) {
  // The spread rect and offsets should be expressed in terms of sigma for the
  // shader.
  float offset_scale = kBlurDistance / (kBlurExtentInSigmas * blur_sigma_);
  spread_rect_.Scale(offset_scale, offset_scale);

  // The box shadow is a triangle strip covering the area between outer rect
  // and inner rect.
  if (inner_rect.IsEmpty()) {
    attributes_square_.reserve(4);
    attributes_square_.emplace_back(outer_rect.x(), outer_rect.y(),
                                    offset_scale);
    attributes_square_.emplace_back(outer_rect.right(), outer_rect.y(),
                                    offset_scale);
    attributes_square_.emplace_back(outer_rect.x(), outer_rect.bottom(),
                                    offset_scale);
    attributes_square_.emplace_back(outer_rect.right(), outer_rect.bottom(),
                                    offset_scale);
  } else {
    math::RectF inside_rect(inner_rect);
    inside_rect.Intersect(outer_rect);
    attributes_square_.reserve(10);
    attributes_square_.emplace_back(outer_rect.x(), outer_rect.y(),
                                    offset_scale);
    attributes_square_.emplace_back(inside_rect.x(), inside_rect.y(),
                                    offset_scale);
    attributes_square_.emplace_back(outer_rect.right(), outer_rect.y(),
                                    offset_scale);
    attributes_square_.emplace_back(inside_rect.right(), inside_rect.y(),
                                    offset_scale);
    attributes_square_.emplace_back(outer_rect.right(), outer_rect.bottom(),
                                    offset_scale);
    attributes_square_.emplace_back(inside_rect.right(), inside_rect.bottom(),
                                    offset_scale);
    attributes_square_.emplace_back(outer_rect.x(), outer_rect.bottom(),
                                    offset_scale);
    attributes_square_.emplace_back(inside_rect.x(), inside_rect.bottom(),
                                    offset_scale);
    attributes_square_.emplace_back(outer_rect.x(), outer_rect.y(),
                                    offset_scale);
    attributes_square_.emplace_back(inside_rect.x(), inside_rect.y(),
                                    offset_scale);
  }

  graphics_state->ReserveVertexData(attributes_square_.size() *
                                    sizeof(attributes_square_[0]));
}

void DrawRectShadowBlur::SetGeometry(GraphicsState* graphics_state,
                                     const RRectAttributes (&rrect)[8]) {
  // The spread rect and offsets should be expressed in terms of sigma for the
  // shader.
  float offset_scale = kBlurDistance / (kBlurExtentInSigmas * blur_sigma_);
  spread_rect_.Scale(offset_scale, offset_scale);
  *spread_corners_ = spread_corners_->Scale(offset_scale, offset_scale);

  // The shadowed area is already split into quads.
  for (int i = 0; i < arraysize(rrect); ++i) {
    AddQuad(rrect[i].bounds, offset_scale, rrect[i].rcorner);
  }

  graphics_state->ReserveVertexData(attributes_round_.size() *
                                    sizeof(attributes_round_[0]));
  graphics_state->ReserveVertexIndices(indices_.size());
}

void DrawRectShadowBlur::SetGeometry(GraphicsState* graphics_state,
                                     const RRectAttributes (&rrect_outer)[4],
                                     const RRectAttributes (&rrect_inner)[8]) {
  // The spread rect and offsets should be expressed in terms of sigma for the
  // shader.
  float offset_scale = kBlurDistance / (kBlurExtentInSigmas * blur_sigma_);
  spread_rect_.Scale(offset_scale, offset_scale);
  *spread_corners_ = spread_corners_->Scale(offset_scale, offset_scale);

  // Draw the area between the inner rect and outer rect using the outer rect's
  // rounded corners. The inner quads already exclude the inscribed rectangle.
  for (int i = 0; i < arraysize(rrect_inner); ++i) {
    for (int o = 0; o < arraysize(rrect_outer); ++o) {
      math::RectF rect =
          math::IntersectRects(rrect_inner[i].bounds, rrect_outer[o].bounds);
      if (!rect.IsEmpty()) {
        AddQuad(rect, offset_scale, rrect_outer[o].rcorner);
      }
    }
  }

  graphics_state->ReserveVertexData(attributes_round_.size() *
                                    sizeof(attributes_round_[0]));
  graphics_state->ReserveVertexIndices(indices_.size());
}

void DrawRectShadowBlur::AddQuad(const math::RectF& rect, float scale,
                                 const RCorner& rcorner) {
  uint16_t vert = static_cast<uint16_t>(attributes_round_.size());
  attributes_round_.emplace_back(rect.x(), rect.y(), scale, rcorner);
  attributes_round_.emplace_back(rect.right(), rect.y(), scale, rcorner);
  attributes_round_.emplace_back(rect.x(), rect.bottom(), scale, rcorner);
  attributes_round_.emplace_back(rect.right(), rect.bottom(), scale, rcorner);
  indices_.emplace_back(vert);
  indices_.emplace_back(vert + 1);
  indices_.emplace_back(vert + 2);
  indices_.emplace_back(vert + 1);
  indices_.emplace_back(vert + 2);
  indices_.emplace_back(vert + 3);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
