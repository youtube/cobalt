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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_SHADOW_SPREAD_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_SHADOW_SPREAD_H_

#include <vector>

#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/renderer/rasterizer/egl/draw_object.h"
#include "egl/generated_shader_impl.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Example CSS box shadow (outset):
//   +-------------------------------------+
//   | Box shadow "blur" region            |
//   |   +-----------------------------+   |
//   |   | Box shadow "spread" region  |   |
//   |   |   +---------------------+   |   |
//   |   |   | Box shadow rect     |   |   |
//   |   |   | (exclude geometry)  |   |   |
//   |   |   +---------------------+   |   |
//   |   |                             |   |
//   |   +-----------------------------+   |
//   |                                     |
//   +-------------------------------------+

// Handles drawing the solid "spread" portion of a box shadow.
class DrawRectShadowSpread : public DrawObject {
 public:
  // Fill the area between |inner_rect| and |outer_rect| with the specified
  // color.
  DrawRectShadowSpread(GraphicsState* graphics_state,
                       const BaseState& base_state,
                       const math::RectF& inner_rect,
                       const OptionalRoundedCorners& inner_corners,
                       const math::RectF& outer_rect,
                       const OptionalRoundedCorners& outer_corners,
                       const render_tree::ColorRGBA& color);

  void ExecuteUpdateVertexBuffer(
      GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) override;
  void ExecuteRasterize(GraphicsState* graphics_state,
                        ShaderProgramManager* program_manager) override;
  base::TypeId GetTypeId() const override;

 private:
  struct VertexAttributesSquare {
    VertexAttributesSquare(float x, float y, uint32_t color);
    float position[2];
    uint32_t color;
  };

  struct VertexAttributesRound {
    VertexAttributesRound(float x, float y,
        const RCorner& inner, const RCorner& outer);
    float position[2];
    RCorner rcorner_inner;
    RCorner rcorner_outer;
  };

  void SetupVertexShader(GraphicsState* graphics_state,
                         const ShaderVertexColor& shader);
  void SetupVertexShader(GraphicsState* graphics_state,
                         const ShaderVertexRcorner2& shader);

  void SetGeometry(GraphicsState* graphics_state,
                   const math::RectF& inner_rect,
                   const math::RectF& outer_rect);
  void SetGeometry(GraphicsState* graphics_state,
                   const math::RectF& inner_rect,
                   const render_tree::RoundedCorners& inner_corners,
                   const math::RectF& outer_rect,
                   const render_tree::RoundedCorners& outer_corners);

  render_tree::ColorRGBA color_;

  std::vector<VertexAttributesSquare> attributes_square_;
  std::vector<VertexAttributesRound> attributes_round_;
  std::vector<uint16_t> indices_;

  uint8_t* vertex_buffer_;
  uint16_t* index_buffer_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_SHADOW_SPREAD_H_
