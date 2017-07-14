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
      opacity(other.opacity) {}

DrawObject::DrawObject(const BaseState& base_state)
    : base_state_(base_state) {}

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
    const math::RectF& rect, const render_tree::RoundedCorners& corners) {
  // Inset by half a pixel to center the anti-aliasing on the edges.
  const float kInset = 0.5f;

  // Ensure corner sizes are non-zero to allow generic handling of square and
  // rounded corners.
  const float kMinCornerSize = 0.01f;

  math::RectF inset_rect(rect);
  inset_rect.Inset(kInset, kInset);
  render_tree::RoundedCorners inset_corners =
      corners.Inset(kInset, kInset, kInset, kInset);
  inset_corners.Normalize(inset_rect);

  // The rect data is a vec4 representing (min.xy, max.xy).
  float rect_data[4] = {
    inset_rect.x(), inset_rect.y(), inset_rect.right(), inset_rect.bottom(),
  };
  GL_CALL(glUniform4fv(rect_uniform, 1, rect_data));

  // The corners data is a mat4 with each vector representing a corner
  // (ordered top left, top right, bottom left, bottom right). Each corner
  // vec4 represents (start.xy, 1 / radius.xy).
  float corners_data[16] = {
    inset_rect.x() + inset_corners.top_left.horizontal,
    inset_rect.y() + inset_corners.top_left.vertical,
    1.0f / std::max(inset_corners.top_left.horizontal, kMinCornerSize),
    1.0f / std::max(inset_corners.top_left.vertical, kMinCornerSize),
    inset_rect.right() - inset_corners.top_right.horizontal,
    inset_rect.y() + inset_corners.top_right.vertical,
    1.0f / std::max(inset_corners.top_right.horizontal, kMinCornerSize),
    1.0f / std::max(inset_corners.top_right.vertical, kMinCornerSize),
    inset_rect.x() + inset_corners.bottom_left.horizontal,
    inset_rect.bottom() - inset_corners.bottom_left.vertical,
    1.0f / std::max(inset_corners.bottom_left.horizontal, kMinCornerSize),
    1.0f / std::max(inset_corners.bottom_left.vertical, kMinCornerSize),
    inset_rect.right() - inset_corners.bottom_right.horizontal,
    inset_rect.bottom() - inset_corners.bottom_right.vertical,
    1.0f / std::max(inset_corners.bottom_right.horizontal, kMinCornerSize),
    1.0f / std::max(inset_corners.bottom_right.vertical, kMinCornerSize),
  };
  GL_CALL(glUniformMatrix4fv(corners_uniform, 1, false, corners_data));
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
