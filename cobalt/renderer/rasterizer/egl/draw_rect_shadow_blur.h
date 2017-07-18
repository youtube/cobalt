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

#include "cobalt/renderer/rasterizer/egl/draw_rect_shadow_spread.h"

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
//
// This uses a shader to mimic skia's SkBlurMask.cpp.
// See also http://stereopsis.com/shadowrect/ as reference for the formula
// used to approximate the gaussian integral (which controls the opacity of
// the shadow).
class DrawRectShadowBlur : public DrawRectShadowSpread {
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

  void ExecuteRasterize(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) OVERRIDE;
  base::TypeId GetTypeId() const OVERRIDE;

 private:
  math::RectF spread_rect_;
  OptionalRoundedCorners spread_corners_;

  float blur_sigma_;
  bool is_inset_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_SHADOW_BLUR_H_
