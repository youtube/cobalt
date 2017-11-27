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

#include <vector>

#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/renderer/rasterizer/egl/draw_object.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Handles drawing a rectangle with a linear color gradient. This may use
// transparency to mask out unwanted pixels, so it must be processed with other
// transparent draws.
class DrawRectLinearGradient : public DrawObject {
 public:
  DrawRectLinearGradient(GraphicsState* graphics_state,
                         const BaseState& base_state,
                         const math::RectF& rect,
                         const render_tree::LinearGradientBrush& brush);

  void ExecuteUpdateVertexBuffer(
      GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) override;
  void ExecuteRasterize(GraphicsState* graphics_state,
                        ShaderProgramManager* program_manager) override;
  base::TypeId GetTypeId() const override;

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
  void AddVertex(float x, float y, uint32_t color);

  // For angled gradients, this is the rotation needed to transform a
  // horizontal gradient into the desired rect with angled gradient. The
  // include scissor will be used to ensure that only the desired pixels
  // are modified.
  math::Matrix3F transform_;
  math::RectF include_scissor_;

  struct VertexAttributes {
    float position[2];
    float offset[2];
    uint32_t color;
  };
  std::vector<VertexAttributes> attributes_;

  uint8_t* vertex_buffer_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_LINEAR_GRADIENT_H_
