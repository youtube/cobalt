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

#include "cobalt/renderer/rasterizer/egl/draw_rect_border.h"

#include "base/logging.h"
#include "cobalt/math/insets_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "cobalt/renderer/rasterizer/common/utils.h"
#include "egl/generated_shader_impl.h"
#include "starboard/memory.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {
// The border is drawn using three regions: an antialiased outer area, an
// antialiased inner area, and the solid area in between.
const int kRegionCount = 3;

// Each region consists of an outer and inner rectangle. However, since regions
// are adjacent to each other, many of these rectangles are shared.
const int kVertexCount = (kRegionCount + 1) * 4;

// The draw object may draw the content rect as well. If so, two triangles are
// used to draw the content rect.
const int kIndexCountForContentRect = 2 * 3;

// Each region has 4 rectangular areas corresponding to the possible borders.
// Each rectangular area is drawn using 2 triangles.
const int kIndexCount = kRegionCount * (4 * 2 * 3) + kIndexCountForContentRect;
}  // namespace

DrawRectBorder::DrawRectBorder(GraphicsState* graphics_state,
                               const BaseState& base_state,
                               const scoped_refptr<render_tree::RectNode>& node)
    : DrawPolyColor(base_state), draw_content_rect_(false) {
  DCHECK(node->data().border);
  allow_simple_clip_ = true;

  const render_tree::Border& border = *(node->data().border);
  render_tree::ColorRGBA border_color;
  int num_borders = 0;
  int num_unhandled_borders = 0;
  bool uniform_opposing_borders = border.left.style == border.right.style &&
                                  border.left.color == border.right.color &&
                                  border.top.style == border.bottom.style &&
                                  border.top.color == border.bottom.color;
  bool uniform_borders = uniform_opposing_borders &&
                         border.left.style == border.top.style &&
                         border.left.color == border.top.color;

  if (border.left.style == render_tree::kBorderStyleSolid) {
    border_color = border.left.color;
    ++num_borders;
  } else if (border.left.width > 0.0f) {
    ++num_unhandled_borders;
  }

  if (border.right.style == render_tree::kBorderStyleSolid) {
    border_color = border.right.color;
    ++num_borders;
  } else if (border.right.width > 0.0f) {
    ++num_unhandled_borders;
  }

  if (border.top.style == render_tree::kBorderStyleSolid) {
    border_color = border.top.color;
    ++num_borders;
  } else if (border.top.width > 0.0f) {
    ++num_unhandled_borders;
  }

  if (border.bottom.style == render_tree::kBorderStyleSolid) {
    border_color = border.bottom.color;
    ++num_borders;
  } else if (border.bottom.width > 0.0f) {
    ++num_unhandled_borders;
  }

  // Only non-rounded borders are supported. Additionally, only borders that
  // do not create a diagonal edge are supported. For example, a top border of
  // one color adjacent to a right border of a different color would create a
  // diagonal edge between them.
  is_valid_ =
      !node->data().rounded_corners && num_unhandled_borders == 0 &&
      ((num_borders <= 1) || (num_borders == 2 && uniform_opposing_borders) ||
       (num_borders == 4 && uniform_borders));

  // If the background brush is solid-colored, then this object can handle the
  // content rect as well. Otherwise, don't draw the inner antialiased edge to
  // avoid having to blend with an unknown color.
  render_tree::ColorRGBA content_color(0);
  if (is_valid_) {
    if (node->data().background_brush) {
      draw_content_rect_ = node->data().background_brush->GetTypeId() ==
                           base::GetTypeId<render_tree::SolidColorBrush>();
      if (draw_content_rect_) {
        const render_tree::SolidColorBrush* solid_brush =
            base::polymorphic_downcast<const render_tree::SolidColorBrush*>(
                node->data().background_brush.get());
        content_color =
            GetDrawColor(solid_brush->color()) * base_state_.opacity;
      }
    } else {
      // No background brush is the same as a totally transparent background.
      draw_content_rect_ = true;
    }
  }

  if (is_valid_) {
    content_rect_ = node->data().rect;
    content_rect_.Inset(border.left.width, border.top.width, border.right.width,
                        border.bottom.width);
    node_bounds_ = node->data().rect;

    if (num_borders > 0) {
      attributes_.reserve(kVertexCount);
      indices_.reserve(kIndexCount);
      border_color = GetDrawColor(border_color) * base_state_.opacity;
      is_valid_ = SetSquareBorder(border, node->data().rect, content_rect_,
                                  border_color, content_color);
      if (is_valid_ && attributes_.size() > 0) {
        graphics_state->ReserveVertexData(attributes_.size() *
                                          sizeof(attributes_[0]));
        graphics_state->ReserveVertexIndices(indices_.size());
      }
    }
  }
}

bool DrawRectBorder::SetSquareBorder(
    const render_tree::Border& border, const math::RectF& border_rect,
    const math::RectF& content_rect, const render_tree::ColorRGBA& border_color,
    const render_tree::ColorRGBA& content_color) {
  // If the scaled border rect is too small, then don't bother rendering.
  math::Vector2dF scale = math::GetScale2d(base_state_.transform);
  if (border_rect.width() * scale.x() < 1.0f ||
      border_rect.height() * scale.y() < 1.0f) {
    return true;
  }

  // Antialiased subpixel borders are not supported at this time. It can be
  // done by attenuating the alpha, but this can get complicated if the borders
  // are of different widths.
  float pixel_size_x = 1.0f / scale.x();
  float pixel_size_y = 1.0f / scale.y();
  if ((border.left.style != render_tree::kBorderStyleNone &&
       border.left.width < pixel_size_x) ||
      (border.right.style != render_tree::kBorderStyleNone &&
       border.right.width < pixel_size_x) ||
      (border.top.style != render_tree::kBorderStyleNone &&
       border.top.width < pixel_size_y) ||
      (border.bottom.style != render_tree::kBorderStyleNone &&
       border.bottom.style < pixel_size_y)) {
    return false;
  }

  // To antialias the edges, shrink the borders by half a pixel, then add a
  // 1-pixel edge which transitions to 0 (for outer) or content_color (for
  // inner). The rasterizer will handle interpolating to the correct color.
  math::InsetsF insets(
      border.left.style != render_tree::kBorderStyleNone ? 1.0f : 0.0f,
      border.top.style != render_tree::kBorderStyleNone ? 1.0f : 0.0f,
      border.right.style != render_tree::kBorderStyleNone ? 1.0f : 0.0f,
      border.bottom.style != render_tree::kBorderStyleNone ? 1.0f : 0.0f);
  math::RectF outer_rect(border_rect);
  math::RectF inner_rect(content_rect);
  outer_rect.Inset(insets.Scale(0.5f * pixel_size_x, 0.5f * pixel_size_y));
  const bool content_is_visible =
      !common::utils::IsTransparent(content_color.a());
  if (draw_content_rect_ && content_is_visible) {
    inner_rect.Inset(insets.Scale(-0.5f * pixel_size_x, -0.5f * pixel_size_y));
  }
  math::RectF outer_outer(outer_rect);
  math::RectF inner_inner(inner_rect);
  outer_outer.Inset(insets.Scale(-pixel_size_x, -pixel_size_y));
  if (draw_content_rect_ && content_is_visible) {
    inner_inner.Inset(insets.Scale(pixel_size_x, pixel_size_y));
  }

  // Add the vertex attributes for the rectangles that will be used.
  uint16_t outer_outer_verts = static_cast<uint16_t>(attributes_.size());
  AddRectVertices(outer_outer, 0);
  uint16_t outer_rect_verts = static_cast<uint16_t>(attributes_.size());
  AddRectVertices(outer_rect, GetGLRGBA(border_color));
  uint16_t inner_rect_verts = static_cast<uint16_t>(attributes_.size());
  AddRectVertices(inner_rect, GetGLRGBA(border_color));
  uint16_t inner_inner_verts = inner_rect_verts;
  if (draw_content_rect_ && content_is_visible) {
    inner_inner_verts = static_cast<uint16_t>(attributes_.size());
    AddRectVertices(inner_inner, GetGLRGBA(content_color));
  }

  // Add indices to draw the borders using the vertex attributes added.
  AddBorders(border, outer_outer_verts, outer_rect_verts);
  AddBorders(border, outer_rect_verts, inner_rect_verts);
  if (draw_content_rect_ && content_is_visible) {
    AddBorders(border, inner_rect_verts, inner_inner_verts);
  }

  // Draw the content rect as appropriate.
  if (draw_content_rect_ && content_is_visible) {
    AddRectIndices(inner_inner_verts, inner_inner_verts + 1,
                   inner_inner_verts + 2, inner_inner_verts + 3);
  }

  // Update the content and node bounds to account for the antialiasing edges.
  node_bounds_ = outer_outer;
  content_rect_ = inner_inner;
  return true;
}

void DrawRectBorder::AddBorders(const render_tree::Border& border,
                                uint16_t outer_verts, uint16_t inner_verts) {
  // Draw the area between those two rectangles using triangle primitives.
  // The vertices for the rectangles were added as top-left, top-right,
  // bottom-left, and bottom-right. See DrawPolyColor::AddRectVertices().
  if (border.left.style != render_tree::kBorderStyleNone) {
    AddRectIndices(outer_verts, inner_verts, outer_verts + 2, inner_verts + 2);
  }
  if (border.top.style != render_tree::kBorderStyleNone) {
    AddRectIndices(outer_verts, outer_verts + 1, inner_verts, inner_verts + 1);
  }
  if (border.right.style != render_tree::kBorderStyleNone) {
    AddRectIndices(inner_verts + 1, outer_verts + 1, inner_verts + 3,
                   outer_verts + 3);
  }
  if (border.bottom.style != render_tree::kBorderStyleNone) {
    AddRectIndices(inner_verts + 2, inner_verts + 3, outer_verts + 2,
                   outer_verts + 3);
  }
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
