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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_LINEAR_GRADIENT_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_LINEAR_GRADIENT_H_

#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/renderer/rasterizer/egl/draw_depth_stencil.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Handles drawing a rectangle with a linear color gradient. This may use a
// depth stencil, so it must be processed with other transparent draws.
class DrawRectLinearGradient : public DrawDepthStencil {
 public:
  DrawRectLinearGradient(GraphicsState* graphics_state,
                         const BaseState& base_state,
                         const math::RectF& rect,
                         const render_tree::LinearGradientBrush& brush);

  void ExecuteOnscreenRasterize(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) OVERRIDE;

 private:
  void AddRectWithHorizontalGradient(
      const math::RectF& rect, const math::PointF& source,
      const math::PointF& dest, const render_tree::ColorStopList& color_stops);
  void AddRectWithVerticalGradient(
      const math::RectF& rect, const math::PointF& source,
      const math::PointF& dest, const render_tree::ColorStopList& color_stops);
  void AddRectWithAngledGradient(
      const math::RectF& rect, const render_tree::LinearGradientBrush& brush);
  uint32_t GetGLColor(const render_tree::ColorStop& color_stop);

  // Index of the first vertex for the rect with gradient.
  size_t first_rect_vert_;

  // For angled gradients, this is the rotation angle needed to transform the
  // submitted rect with horizontal gradient into the desired rect with angled
  // gradient.
  float gradient_angle_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_LINEAR_GRADIENT_H_
