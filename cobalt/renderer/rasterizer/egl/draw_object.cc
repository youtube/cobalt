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
    const math::RectF& rect, const render_tree::RoundedCorners& corners,
    float inset) {
  math::RectF inset_rect(rect);
  inset_rect.Inset(inset, inset);
  render_tree::RoundedCorners inset_corners =
      corners.Inset(inset, inset, inset, inset);

  // Tweak corners that are square-ish so they have values that play
  // nicely with the shader. Interpolating x^2 / a^2 + y^2 / b^2 does not
  // work well when |a| or |b| are very small.
  if (inset_corners.top_left.horizontal <= 0.5f ||
      inset_corners.top_left.vertical <= 0.5f) {
    inset_corners.top_left.horizontal = 0.0f;
    inset_corners.top_left.vertical = 0.0f;
  }
  if (inset_corners.top_right.horizontal <= 0.5f ||
      inset_corners.top_right.vertical <= 0.5f) {
    inset_corners.top_right.horizontal = 0.0f;
    inset_corners.top_right.vertical = 0.0f;
  }
  if (inset_corners.bottom_left.horizontal <= 0.5f ||
      inset_corners.bottom_left.vertical <= 0.5f) {
    inset_corners.bottom_left.horizontal = 0.0f;
    inset_corners.bottom_left.vertical = 0.0f;
  }
  if (inset_corners.bottom_right.horizontal <= 0.5f ||
      inset_corners.bottom_right.vertical <= 0.5f) {
    inset_corners.bottom_right.horizontal = 0.0f;
    inset_corners.bottom_right.vertical = 0.0f;
  }

  // Ensure corner sizes are non-zero to allow generic handling of square and
  // rounded corners.
  const float kMinCornerSize = 0.01f;
  inset_rect.Outset(kMinCornerSize, kMinCornerSize);
  inset_corners = inset_corners.Inset(-kMinCornerSize, -kMinCornerSize,
      -kMinCornerSize, -kMinCornerSize);
  inset_corners = inset_corners.Normalize(inset_rect);

  // The rect data is a vec4 representing (min.xy, max.xy).
  float rect_data[4] = {
    inset_rect.x(), inset_rect.y(), inset_rect.right(), inset_rect.bottom(),
  };
  GL_CALL(glUniform4fv(rect_uniform, 1, rect_data));

  // The corners data is a mat4 with each vector representing a corner
  // (ordered top left, top right, bottom left, bottom right). Each corner
  // vec4 represents (start.xy, radius.xy).
  float corners_data[16] = {
    inset_rect.x() + inset_corners.top_left.horizontal,
    inset_rect.y() + inset_corners.top_left.vertical,
    inset_corners.top_left.horizontal,
    inset_corners.top_left.vertical,
    inset_rect.right() - inset_corners.top_right.horizontal,
    inset_rect.y() + inset_corners.top_right.vertical,
    inset_corners.top_right.horizontal,
    inset_corners.top_right.vertical,
    inset_rect.x() + inset_corners.bottom_left.horizontal,
    inset_rect.bottom() - inset_corners.bottom_left.vertical,
    inset_corners.bottom_left.horizontal,
    inset_corners.bottom_left.vertical,
    inset_rect.right() - inset_corners.bottom_right.horizontal,
    inset_rect.bottom() - inset_corners.bottom_right.vertical,
    inset_corners.bottom_right.horizontal,
    inset_corners.bottom_right.vertical,
  };
  GL_CALL(glUniformMatrix4fv(corners_uniform, 1, false, corners_data));
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
