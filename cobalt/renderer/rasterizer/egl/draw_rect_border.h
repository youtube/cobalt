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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_BORDER_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_BORDER_H_

#include <vector>

#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/renderer/rasterizer/egl/draw_poly_color.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Handles drawing the antialiased border for a RectNode. Only certain border
// settings are supported, so check DrawRectBorder::IsValid to verify support.
class DrawRectBorder : public DrawPolyColor {
 public:
  DrawRectBorder(GraphicsState* graphics_state, const BaseState& base_state,
                 const scoped_refptr<render_tree::RectNode>& node);

  bool IsValid() const { return is_valid_; }
  bool DrawsContentRect() const { return draw_content_rect_; }
  const math::RectF& GetContentRect() const { return content_rect_; }
  const math::RectF& GetBounds() const { return node_bounds_; }

 private:
  bool SetSquareBorder(const render_tree::Border& border,
                       const math::RectF& border_rect,
                       const math::RectF& content_rect,
                       const render_tree::ColorRGBA& border_color,
                       const render_tree::ColorRGBA& content_color);
  void AddBorders(const render_tree::Border& border, uint16_t outer_verts,
                  uint16_t inner_verts);

  math::RectF content_rect_;
  math::RectF node_bounds_;
  bool is_valid_;
  bool draw_content_rect_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_BORDER_H_
