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

#include "cobalt/renderer/rasterizer/egl/draw_object.h"

#include <algorithm>
#include <limits>

#include "cobalt/math/transform_2d.h"
#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

DrawObject::BaseState::BaseState()
    : transform(math::Matrix3F::Identity()),
      scissor(0, 0,
              std::numeric_limits<int>::max(),
              std::numeric_limits<int>::max()),
      opacity(1.0f) {}

DrawObject::BaseState::BaseState(const BaseState& other)
    : transform(other.transform),
      scissor(other.scissor),
      rounded_scissor_rect(other.rounded_scissor_rect),
      rounded_scissor_corners(other.rounded_scissor_corners),
      opacity(other.opacity) {}

DrawObject::DrawObject(const BaseState& base_state)
    : base_state_(base_state) {}

math::Vector2dF DrawObject::GetScale() const {
  float m00 = base_state_.transform(0, 0);
  float m01 = base_state_.transform(0, 1);
  float m10 = base_state_.transform(1, 0);
  float m11 = base_state_.transform(1, 1);
  return math::Vector2dF(std::sqrt(m00 * m00 + m10 * m10),
                         std::sqrt(m01 * m01 + m11 * m11));
}

math::Vector2dF DrawObject::RemoveScaleFromTransform() {
  // Avoid division by zero.
  const float kEpsilon = 0.00001f;

  math::Vector2dF scale = GetScale();
  base_state_.transform = base_state_.transform *
      math::ScaleMatrix(1.0f / std::max(scale.x(), kEpsilon),
                        1.0f / std::max(scale.y(), kEpsilon));
  return scale;
}

// static
uint32_t DrawObject::GetGLRGBA(float r, float g, float b, float a) {
  // Ensure color bytes represent RGBA, regardless of endianness.
  union {
    uint32_t color32;
    uint8_t color8[4];
  } color_data;
  color_data.color8[0] = static_cast<uint8_t>(r * 255.0f);
  color_data.color8[1] = static_cast<uint8_t>(g * 255.0f);
  color_data.color8[2] = static_cast<uint8_t>(b * 255.0f);
  color_data.color8[3] = static_cast<uint8_t>(a * 255.0f);
  return color_data.color32;
}

// static
void DrawObject::SetRRectUniforms(GLint rect_uniform, GLint corners_uniform,
    math::RectF rect, render_tree::RoundedCorners corners) {
  // Ensure corner sizes are non-zero to allow generic handling of square and
  // rounded corners.
  const float kMinCornerSize = 0.01f;
  rect.Outset(kMinCornerSize, kMinCornerSize);
  corners = corners.Inset(-kMinCornerSize, -kMinCornerSize, -kMinCornerSize,
                          -kMinCornerSize);

  // The rect data is a vec4 representing (min.xy, max.xy).
  GL_CALL(glUniform4f(rect_uniform, rect.x(), rect.y(), rect.right(),
                      rect.bottom()));

  // The corners data is a mat4 with each vector representing a corner
  // (ordered top left, top right, bottom left, bottom right). Each corner
  // vec4 represents (start.xy, 1 / radius.xy).
  float corners_data[16] = {
    rect.x() + corners.top_left.horizontal,
    rect.y() + corners.top_left.vertical,
    1.0f / corners.top_left.horizontal,
    1.0f / corners.top_left.vertical,
    rect.right() - corners.top_right.horizontal,
    rect.y() + corners.top_right.vertical,
    1.0f / corners.top_right.horizontal,
    1.0f / corners.top_right.vertical,
    rect.x() + corners.bottom_left.horizontal,
    rect.bottom() - corners.bottom_left.vertical,
    1.0f / corners.bottom_left.horizontal,
    1.0f / corners.bottom_left.vertical,
    rect.right() - corners.bottom_right.horizontal,
    rect.bottom() - corners.bottom_right.vertical,
    1.0f / corners.bottom_right.horizontal,
    1.0f / corners.bottom_right.vertical,
  };
  GL_CALL(glUniformMatrix4fv(corners_uniform, 1, false, corners_data));
}

// static
void DrawObject::GetRRectAttributes(const math::RectF& bounds,
    math::RectF rect, render_tree::RoundedCorners corners,
    RRectAttributes out_attributes[4]) {
  // Ensure corner sizes are non-zero to allow generic handling of square and
  // rounded corners. Corner radii must be at least 1 pixel for antialiasing
  // to work well.
  const float kMinCornerSize = 1.0f;

  // First inset to make room for the minimum corner size. Then outset to
  // enforce the minimum corner size.
  rect.Inset(std::min(rect.width() * 0.5f, kMinCornerSize),
             std::min(rect.height() * 0.5f, kMinCornerSize));
  corners = corners.Inset(kMinCornerSize, kMinCornerSize, kMinCornerSize,
                          kMinCornerSize);
  corners = corners.Normalize(rect);
  rect.Outset(kMinCornerSize, kMinCornerSize);
  corners = corners.Inset(-kMinCornerSize, -kMinCornerSize, -kMinCornerSize,
                          -kMinCornerSize);

  // |rcorner| describes (start.xy, 1 / radius.xy) for the relevant corner.
  // The sign of the radius component is used to facilitate the calculation:
  //   vec2 scaled_offset = (position - corner.xy) * corner.zw
  // Such that |scaled_offset| is in the first quadrant when the pixel is
  // in the given rounded corner.
  COMPILE_ASSERT(sizeof(RCorner) == sizeof(float) * 4, struct_should_be_vec4);
  out_attributes[0].rcorner.x = rect.x() + corners.top_left.horizontal;
  out_attributes[0].rcorner.y = rect.y() + corners.top_left.vertical;
  out_attributes[0].rcorner.rx = -1.0f / corners.top_left.horizontal;
  out_attributes[0].rcorner.ry = -1.0f / corners.top_left.vertical;

  out_attributes[1].rcorner.x = rect.right() - corners.top_right.horizontal;
  out_attributes[1].rcorner.y = rect.y() + corners.top_right.vertical;
  out_attributes[1].rcorner.rx = 1.0f / corners.top_right.horizontal;
  out_attributes[1].rcorner.ry = -1.0f / corners.top_right.vertical;

  out_attributes[2].rcorner.x = rect.x() + corners.bottom_left.horizontal;
  out_attributes[2].rcorner.y = rect.bottom() - corners.bottom_left.vertical;
  out_attributes[2].rcorner.rx = -1.0f / corners.bottom_left.horizontal;
  out_attributes[2].rcorner.ry = 1.0f / corners.bottom_left.vertical;

  out_attributes[3].rcorner.x = rect.right() - corners.bottom_right.horizontal;
  out_attributes[3].rcorner.y = rect.bottom() - corners.bottom_right.vertical;
  out_attributes[3].rcorner.rx = 1.0f / corners.bottom_right.horizontal;
  out_attributes[3].rcorner.ry = 1.0f / corners.bottom_right.vertical;

  // Calculate the bounds for each patch. A normalized rounded rect should have
  // a center point which none of the corners cross.
  math::PointF center(
    0.5f * (std::max(out_attributes[0].rcorner.x,
                     out_attributes[2].rcorner.x) +
            std::min(out_attributes[1].rcorner.x,
                     out_attributes[3].rcorner.x)),
    0.5f * (std::max(out_attributes[0].rcorner.y,
                     out_attributes[1].rcorner.y) +
            std::min(out_attributes[2].rcorner.y,
                     out_attributes[3].rcorner.y)));

  // The bounds may not fully contain the rounded rect.
  out_attributes[0].bounds.SetRect(
      bounds.x(), bounds.y(),
      std::max(center.x() - bounds.x(), 0.0f),
      std::max(center.y() - bounds.y(), 0.0f));
  out_attributes[1].bounds.SetRect(
      center.x(), bounds.y(),
      std::max(bounds.right() - center.x(), 0.0f),
      std::max(center.y() - bounds.y(), 0.0f));
  out_attributes[2].bounds.SetRect(
      bounds.x(), center.y(),
      std::max(center.x() - bounds.x(), 0.0f),
      std::max(bounds.bottom() - center.y(), 0.0f));
  out_attributes[3].bounds.SetRect(
      center.x(), center.y(),
      std::max(bounds.right() - center.x(), 0.0f),
      std::max(bounds.bottom() - center.y(), 0.0f));
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
