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

#include "cobalt/renderer/rasterizer/egl/draw_object.h"

#include <algorithm>
#include <limits>

#include "cobalt/math/transform_2d.h"
#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
// To accommodate large radius values for rounded corners while using mediump
// floats in the fragment shader, scale 1 / radius.xy by kRCornerGradientScale.
// The fragment shader will take this into account.
const float kRCornerGradientScale = 16.0f;

// Get the midpoint of the given rounded rect. A normalized rounded rect
// should have at least one point which the corners do not cross.
math::PointF GetRRectCenter(const math::RectF& rect,
                            const render_tree::RoundedCorners& corners) {
  return math::PointF(
      0.5f * (rect.x() +
              std::max(corners.top_left.horizontal,
                       corners.bottom_left.horizontal) +
              rect.right() -
              std::max(corners.top_right.horizontal,
                       corners.bottom_right.horizontal)),
      0.5f * (rect.y() +
              std::max(corners.top_left.vertical, corners.top_right.vertical) +
              rect.bottom() -
              std::max(corners.bottom_left.vertical,
                       corners.bottom_right.vertical)));
}

math::PointF ClampToBounds(const math::RectF& bounds, float x, float y) {
  return math::PointF(std::min(std::max(bounds.x(), x), bounds.right()),
                      std::min(std::max(bounds.y(), y), bounds.bottom()));
}
}  // namespace

DrawObject::BaseState::BaseState()
    : transform(math::Matrix3F::Identity()),
      scissor(0, 0, std::numeric_limits<int>::max(),
              std::numeric_limits<int>::max()),
      opacity(1.0f) {}

DrawObject::BaseState::BaseState(const BaseState& other)
    : transform(other.transform),
      scissor(other.scissor),
      rounded_scissor_rect(other.rounded_scissor_rect),
      rounded_scissor_corners(other.rounded_scissor_corners),
      opacity(other.opacity) {}

DrawObject::RCorner::RCorner(const float (&position)[2], const RCorner& init)
    : x((position[0] - init.x) * init.rx),
      y((position[1] - init.y) * init.ry),
      rx(init.rx * kRCornerGradientScale),
      ry(init.ry * kRCornerGradientScale) {}

DrawObject::DrawObject() : merge_type_(base::GetTypeId<DrawObject>()) {}

DrawObject::DrawObject(const BaseState& base_state)
    : base_state_(base_state), merge_type_(base::GetTypeId<DrawObject>()) {}

math::Vector2dF DrawObject::RemoveScaleFromTransform() {
  // Avoid division by zero.
  const float kEpsilon = 0.00001f;

  math::Vector2dF scale = math::GetScale2d(base_state_.transform);
  base_state_.transform =
      base_state_.transform *
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
void DrawObject::GetRRectAttributes(const math::RectF& bounds, math::RectF rect,
                                    render_tree::RoundedCorners corners,
                                    RRectAttributes (&out_attributes)[4]) {
  GetRCornerValues(&rect, &corners, out_attributes);

  // Calculate the bounds for each patch. Four patches will be used to cover
  // the entire bounded area.
  math::PointF center = GetRRectCenter(rect, corners);
  center = ClampToBounds(bounds, center.x(), center.y());

  out_attributes[0].bounds.SetRect(
      bounds.x(), bounds.y(), center.x() - bounds.x(), center.y() - bounds.y());
  out_attributes[1].bounds.SetRect(center.x(), bounds.y(),
                                   bounds.right() - center.x(),
                                   center.y() - bounds.y());
  out_attributes[2].bounds.SetRect(bounds.x(), center.y(),
                                   center.x() - bounds.x(),
                                   bounds.bottom() - center.y());
  out_attributes[3].bounds.SetRect(center.x(), center.y(),
                                   bounds.right() - center.x(),
                                   bounds.bottom() - center.y());
}

// static
void DrawObject::GetRRectAttributes(const math::RectF& bounds, math::RectF rect,
                                    render_tree::RoundedCorners corners,
                                    RRectAttributes (&out_attributes)[8]) {
  GetRCornerValues(&rect, &corners, out_attributes);
  out_attributes[4].rcorner = out_attributes[0].rcorner;
  out_attributes[5].rcorner = out_attributes[1].rcorner;
  out_attributes[6].rcorner = out_attributes[2].rcorner;
  out_attributes[7].rcorner = out_attributes[3].rcorner;

  // Given an ellipse with radii A and B, the largest inscribed rectangle has
  // dimensions sqrt(2) * A and sqrt(2) * B. To accommodate the antialiased
  // edge, inset the inscribed rect by a pixel on each side.
  const float kInsetScale = 0.2929f;  // 1 - sqrt(2) / 2

  // Calculate the bounds for each patch. Eight patches will be used to exclude
  // the inscribed rect:
  //   +---+-----+-----+---+
  //   |   |  4  |  5  |   |
  //   | 0 +-----+-----+ 1 |
  //   |   |           |   |
  //   +---+     C     +---+    C = center point
  //   |   |           |   |
  //   | 2 +-----+-----+ 3 |
  //   |   |  6  |  7  |   |
  //   +---+-----+-----+---+
  math::PointF center = GetRRectCenter(rect, corners);
  center = ClampToBounds(bounds, center.x(), center.y());
  math::PointF inset0 = ClampToBounds(
      bounds, rect.x() + kInsetScale * corners.top_left.horizontal + 1.0f,
      rect.y() + kInsetScale * corners.top_left.vertical + 1.0f);
  math::PointF inset1 = ClampToBounds(
      bounds, rect.right() - kInsetScale * corners.top_right.horizontal - 1.0f,
      rect.y() + kInsetScale * corners.top_right.vertical + 1.0f);
  math::PointF inset2 = ClampToBounds(
      bounds, rect.x() + kInsetScale * corners.bottom_left.horizontal + 1.0f,
      rect.bottom() - kInsetScale * corners.bottom_left.vertical - 1.0f);
  math::PointF inset3 = ClampToBounds(
      bounds,
      rect.right() - kInsetScale * corners.bottom_right.horizontal - 1.0f,
      rect.bottom() - kInsetScale * corners.bottom_right.vertical - 1.0f);

  out_attributes[0].bounds.SetRect(
      bounds.x(), bounds.y(), inset0.x() - bounds.x(), center.y() - bounds.y());
  out_attributes[1].bounds.SetRect(inset1.x(), bounds.y(),
                                   bounds.right() - inset1.x(),
                                   center.y() - bounds.y());
  out_attributes[2].bounds.SetRect(bounds.x(), center.y(),
                                   inset2.x() - bounds.x(),
                                   bounds.bottom() - center.y());
  out_attributes[3].bounds.SetRect(inset3.x(), center.y(),
                                   bounds.right() - inset3.x(),
                                   bounds.bottom() - center.y());
  out_attributes[4].bounds.SetRect(
      inset0.x(), bounds.y(), center.x() - inset0.x(), inset0.y() - bounds.y());
  out_attributes[5].bounds.SetRect(
      center.x(), bounds.y(), inset1.x() - center.x(), inset1.y() - bounds.y());
  out_attributes[6].bounds.SetRect(inset2.x(), inset2.y(),
                                   center.x() - inset2.x(),
                                   bounds.bottom() - inset2.y());
  out_attributes[7].bounds.SetRect(center.x(), inset3.y(),
                                   inset3.x() - center.x(),
                                   bounds.bottom() - inset3.y());
}

// static
void DrawObject::GetRCornerValues(math::RectF* rect,
                                  render_tree::RoundedCorners* corners,
                                  RRectAttributes out_rcorners[4]) {
  // Ensure that square corners have dimensions of 0, otherwise they may be
  // rendered as skewed ellipses (in the case where one dimension is 0 but
  // not the other).
  if (corners->top_left.IsSquare()) {
    corners->top_left.horizontal = 0.0f;
    corners->top_left.vertical = 0.0f;
  }
  if (corners->top_right.IsSquare()) {
    corners->top_right.horizontal = 0.0f;
    corners->top_right.vertical = 0.0f;
  }
  if (corners->bottom_right.IsSquare()) {
    corners->bottom_right.horizontal = 0.0f;
    corners->bottom_right.vertical = 0.0f;
  }
  if (corners->bottom_left.IsSquare()) {
    corners->bottom_left.horizontal = 0.0f;
    corners->bottom_left.vertical = 0.0f;
  }

  // Ensure corner sizes are non-zero to allow generic handling of square and
  // rounded corners. Corner radii must be at least 1 pixel for antialiasing
  // to work well.
  const float kMinCornerSize = 1.0f;

  // First inset to make room for the minimum corner size. Then outset to
  // enforce the minimum corner size. Be careful not to inset more than the
  // rect size, otherwise the outset rect will be off-centered.
  rect->Inset(std::min(rect->width() * 0.5f, kMinCornerSize),
              std::min(rect->height() * 0.5f, kMinCornerSize));
  *corners = corners->Inset(kMinCornerSize, kMinCornerSize, kMinCornerSize,
                            kMinCornerSize);
  *corners = corners->Normalize(*rect);
  rect->Outset(kMinCornerSize, kMinCornerSize);
  *corners = corners->Inset(-kMinCornerSize, -kMinCornerSize, -kMinCornerSize,
                            -kMinCornerSize);

  // |rcorner| describes (start.xy, 1 / radius.xy) for the relevant corner.
  // The sign of the radius component is used to facilitate the calculation:
  //   vec2 scaled_offset = (position - corner.xy) * corner.zw
  // Such that |scaled_offset| is in the first quadrant when the pixel is
  // in the given rounded corner.
  COMPILE_ASSERT(sizeof(RCorner) == sizeof(float) * 4, struct_should_be_vec4);
  out_rcorners[0].rcorner.x = rect->x() + corners->top_left.horizontal;
  out_rcorners[0].rcorner.y = rect->y() + corners->top_left.vertical;
  out_rcorners[0].rcorner.rx = -1.0f / corners->top_left.horizontal;
  out_rcorners[0].rcorner.ry = -1.0f / corners->top_left.vertical;

  out_rcorners[1].rcorner.x = rect->right() - corners->top_right.horizontal;
  out_rcorners[1].rcorner.y = rect->y() + corners->top_right.vertical;
  out_rcorners[1].rcorner.rx = 1.0f / corners->top_right.horizontal;
  out_rcorners[1].rcorner.ry = -1.0f / corners->top_right.vertical;

  out_rcorners[2].rcorner.x = rect->x() + corners->bottom_left.horizontal;
  out_rcorners[2].rcorner.y = rect->bottom() - corners->bottom_left.vertical;
  out_rcorners[2].rcorner.rx = -1.0f / corners->bottom_left.horizontal;
  out_rcorners[2].rcorner.ry = 1.0f / corners->bottom_left.vertical;

  out_rcorners[3].rcorner.x = rect->right() - corners->bottom_right.horizontal;
  out_rcorners[3].rcorner.y = rect->bottom() - corners->bottom_right.vertical;
  out_rcorners[3].rcorner.rx = 1.0f / corners->bottom_right.horizontal;
  out_rcorners[3].rcorner.ry = 1.0f / corners->bottom_right.vertical;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
