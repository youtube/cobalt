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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_POLY_COLOR_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_POLY_COLOR_H_

#include <vector>

#include "base/optional.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/renderer/rasterizer/egl/draw_object.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Handles drawing various colored polygons.
class DrawPolyColor : public DrawObject {
 public:
  DrawPolyColor(GraphicsState* graphics_state,
                const BaseState& base_state,
                const math::RectF& rect,
                const render_tree::ColorRGBA& color);

  bool TryMerge(DrawObject* other) override;
  void ExecuteUpdateVertexBuffer(
      GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) override;
  void ExecuteRasterize(GraphicsState* graphics_state,
                        ShaderProgramManager* program_manager) override;
  base::TypeId GetTypeId() const override;

 protected:
  explicit DrawPolyColor(const BaseState& base_state);
  void SetupShader(GraphicsState* graphics_state,
                   ShaderProgramManager* program_manager);
  void AddRectVertices(const math::RectF& rect, uint32_t color);
  void AddRectIndices(uint16_t top_left, uint16_t top_right,
                      uint16_t bottom_left, uint16_t bottom_right);
  bool PrepareForMerge();

  struct VertexAttributes {
    VertexAttributes(float x, float y, uint32_t color32) {
      position[0] = x;
      position[1] = y;
      color = color32;
    }
    float position[2];
    uint32_t color;
  };
  std::vector<VertexAttributes> attributes_;
  std::vector<uint16_t> indices_;

  // Specify whether vertex positions are allowed to be clamped according to
  // the scissor rect by PrepareForMerge(). This works properly for shapes with
  // axis-aligned edges, but will lead to distorted shapes in other cases.
  bool allow_simple_clip_;

  uint16_t* index_buffer_;
  uint8_t* vertex_buffer_;
  base::optional<bool> can_merge_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_POLY_COLOR_H_
