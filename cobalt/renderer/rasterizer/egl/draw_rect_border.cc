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

#include "cobalt/renderer/rasterizer/egl/draw_rect_border.h"

#include <GLES2/gl2.h>

#include "base/logging.h"
#include "cobalt/renderer/backend/egl/utils.h"
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

// Each region consists of 2 rects which use 4 vertices each. Each region is
// drawn with 8 triangles, and indices are used to minimize vertex duplication.
const int kVertexCountPerRegion = 4 * 2;
const int kIndexCountPerRegion = 8 * 3;

const int kVertexCount = kRegionCount * kVertexCountPerRegion;
const int kIndexCount = kRegionCount * kIndexCountPerRegion;
}  // namespace

DrawRectBorder::DrawRectBorder(GraphicsState* graphics_state,
    const BaseState& base_state,
    const scoped_refptr<render_tree::RectNode>& node)
    : DrawPolyColor(base_state),
      index_buffer_(NULL) {
  DCHECK(node->data().border);
  const render_tree::Border& border = *(node->data().border);

  // Only uniform, non-rounded borders are supported.
  is_valid_ = !node->data().rounded_corners &&
              border.left.style == border.right.style &&
              border.left.style == border.top.style &&
              border.left.style == border.bottom.style &&
              border.left.color == border.right.color &&
              border.left.color == border.top.color &&
              border.left.color == border.bottom.color;

  // If a background brush is used, then only solid colored ones are supported.
  // This simplifies blending the inner-antialiased border with the content.
  render_tree::ColorRGBA content_color(0);
  if (is_valid_ && node->data().background_brush) {
    is_valid_ = node->data().background_brush->GetTypeId() ==
                base::GetTypeId<render_tree::SolidColorBrush>();
    if (is_valid_) {
      const render_tree::SolidColorBrush* solid_brush =
          base::polymorphic_downcast<const render_tree::SolidColorBrush*>
              (node->data().background_brush.get());
      content_color = GetDrawColor(solid_brush->color()) * base_state_.opacity;
    }
  }

  if (is_valid_) {
    content_rect_ = node->data().rect;
    content_rect_.Inset(border.left.width, border.top.width,
                        border.right.width, border.bottom.width);
    node_bounds_ = node->data().rect;

    if (border.left.style == render_tree::kBorderStyleSolid) {
      attributes_.reserve(kVertexCount);
      indices_.reserve(kIndexCount);
      render_tree::ColorRGBA border_color =
          GetDrawColor(border.left.color) * base_state_.opacity;
      is_valid_ = SetSquareBorder(border, node->data().rect, content_rect_,
                                  border_color, content_color);
      if (is_valid_ && attributes_.size() > 0) {
        graphics_state->ReserveVertexData(
            attributes_.size() * sizeof(VertexAttributes));
        graphics_state->ReserveVertexIndices(indices_.size());
      }
    }
  }
}

void DrawRectBorder::ExecuteUpdateVertexBuffer(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  if (attributes_.size() > 0) {
    vertex_buffer_ = graphics_state->AllocateVertexData(
        attributes_.size() * sizeof(VertexAttributes));
    SbMemoryCopy(vertex_buffer_, &attributes_[0],
                 attributes_.size() * sizeof(VertexAttributes));
    index_buffer_ = graphics_state->AllocateVertexIndices(indices_.size());
    SbMemoryCopy(index_buffer_, &indices_[0],
                 indices_.size() * sizeof(indices_[0]));
  }
}

void DrawRectBorder::ExecuteRasterize(
    GraphicsState* graphics_state,
    ShaderProgramManager* program_manager) {
  if (attributes_.size() > 0) {
    SetupShader(graphics_state, program_manager);
    GL_CALL(glDrawElements(GL_TRIANGLES, indices_.size(), GL_UNSIGNED_SHORT,
        graphics_state->GetVertexIndexPointer(index_buffer_)));
  }
}

bool DrawRectBorder::SetSquareBorder(const render_tree::Border& border,
    const math::RectF& border_rect, const math::RectF& content_rect,
    const render_tree::ColorRGBA& border_color,
    const render_tree::ColorRGBA& content_color) {
  // If the scaled border rect is too small, then don't bother rendering.
  math::Vector2dF scale = GetScale();
  if (border_rect.width() * scale.x() < 1.0f ||
      border_rect.height() * scale.y() < 1.0f) {
    return true;
  }

  // Antialiased subpixel borders are not supported at this time. It can be
  // done by attenuating the alpha, but this can get complicated if the borders
  // are of different widths.
  float pixel_size_x = 1.0f / scale.x();
  float pixel_size_y = 1.0f / scale.y();
  if (border.left.width < pixel_size_x || border.right.width < pixel_size_x ||
      border.top.width < pixel_size_y || border.bottom.width < pixel_size_y) {
    return false;
  }

  // To antialias the edges, shrink the borders by half a pixel, then add a
  // 1-pixel edge which transitions to 0 (for outer) or content_color (for
  // inner). The rasterizer will handle interpolating to the correct color.
  math::RectF outer_rect(border_rect);
  math::RectF inner_rect(content_rect);
  outer_rect.Inset(0.5f * pixel_size_x, 0.5f * pixel_size_y);
  inner_rect.Outset(0.5f * pixel_size_x, 0.5f * pixel_size_y);
  math::RectF outer_outer(outer_rect);
  math::RectF inner_inner(inner_rect);
  outer_outer.Outset(pixel_size_x, pixel_size_y);
  inner_inner.Inset(pixel_size_x, pixel_size_y);

  uint32_t border_color32 = GetGLRGBA(border_color);
  uint32_t content_color32 = GetGLRGBA(content_color);
  AddRegion(outer_outer, 0, outer_rect, border_color32);
  AddRegion(outer_rect, border_color32, inner_rect, border_color32);
  AddRegion(inner_rect, border_color32, inner_inner, content_color32);

  // Update the content and node bounds to account for the antialiasing edges.
  node_bounds_ = outer_outer;
  content_rect_ = inner_inner;
  return true;
}

void DrawRectBorder::AddRegion(
    const math::RectF& outer_rect, uint32_t outer_color,
    const math::RectF& inner_rect, uint32_t inner_color) {
  // Add triangles to render the area between the two rects.
  uint16_t first_vertex = static_cast<uint16_t>(attributes_.size());
  AddVertex(outer_rect.x(), outer_rect.y(), outer_color);
  AddVertex(inner_rect.x(), inner_rect.y(), inner_color);
  AddVertex(outer_rect.right(), outer_rect.y(), outer_color);
  AddVertex(inner_rect.right(), inner_rect.y(), inner_color);
  AddVertex(outer_rect.right(), outer_rect.bottom(), outer_color);
  AddVertex(inner_rect.right(), inner_rect.bottom(), inner_color);
  AddVertex(outer_rect.x(), outer_rect.bottom(), outer_color);
  AddVertex(inner_rect.x(), inner_rect.bottom(), inner_color);

  // Use indices to minimize duplication of vertex data. The last two triangles
  // use the first one or two vertices of the region.
  uint16_t wrap_start = static_cast<uint16_t>(attributes_.size()) - 2;
  for (uint16_t i = first_vertex; i < wrap_start; ++i) {
    indices_.push_back(i);
    indices_.push_back(i + 1);
    indices_.push_back(i + 2);
  }
  indices_.push_back(wrap_start);
  indices_.push_back(wrap_start + 1);
  indices_.push_back(first_vertex);
  indices_.push_back(wrap_start + 1);
  indices_.push_back(first_vertex);
  indices_.push_back(first_vertex + 1);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
