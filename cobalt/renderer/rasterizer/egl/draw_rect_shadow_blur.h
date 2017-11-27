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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_SHADOW_BLUR_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_SHADOW_BLUR_H_

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
//   | (include scissor)                   |
//   +-------------------------------------+
// NOTE: Despite the CSS naming, the actual blur effect starts inside the
// "spread" region.

// Handles drawing a box shadow with blur. This uses a gaussian kernel to fade
// the "blur" region.
class DrawRectShadowBlur : public DrawObject {
 public:
  // Draw a blurred box shadow.
  // The box shadow exists in the area between |base_rect| and |spread_rect|
  // extended (inset or outset accordingly) to cover the blur kernel.
  DrawRectShadowBlur(GraphicsState* graphics_state,
                     const BaseState& base_state,
                     const math::RectF& base_rect,
                     const OptionalRoundedCorners& base_corners,
                     const math::RectF& spread_rect,
                     const OptionalRoundedCorners& spread_corners,
                     const render_tree::ColorRGBA& color,
                     float blur_sigma, bool inset);

  void ExecuteUpdateVertexBuffer(
      GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) override;
  void ExecuteRasterize(GraphicsState* graphics_state,
                        ShaderProgramManager* program_manager) override;
  base::TypeId GetTypeId() const override;

 private:
  struct VertexAttributesSquare {
    VertexAttributesSquare(float x, float y, float offset_scale);
    float position[2];
    float offset[2];
  };

  struct VertexAttributesRound {
    VertexAttributesRound(float x, float y, float offset_scale,
                          const RCorner& rcorner);
    float position[2];
    float offset[2];
    RCorner rcorner_scissor;
  };

  void SetupVertexShader(GraphicsState* graphics_state,
                         const ShaderVertexOffset& shader);
  void SetupVertexShader(GraphicsState* graphics_state,
                         const ShaderVertexOffsetRcorner& shader);
  void SetFragmentUniforms(GLint color_uniform, GLint scale_add_uniform);

  void SetGeometry(GraphicsState* graphics_state,
                   const math::RectF& base_rect,
                   const OptionalRoundedCorners& base_corners);
  void SetGeometry(GraphicsState* graphics_state,
                   const math::RectF& inner_rect,
                   const math::RectF& outer_rect);
  void SetGeometry(GraphicsState* graphics_state,
                   const RRectAttributes (&rrect)[8]);
  void SetGeometry(GraphicsState* graphics_state,
                   const RRectAttributes (&rrect_outer)[4],
                   const RRectAttributes (&rrect_inner)[8]);
  void AddQuad(const math::RectF& rect, float scale, const RCorner& rcorner);

  math::RectF spread_rect_;
  OptionalRoundedCorners spread_corners_;
  render_tree::ColorRGBA color_;
  float blur_sigma_;
  bool is_inset_;

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

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_SHADOW_BLUR_H_
