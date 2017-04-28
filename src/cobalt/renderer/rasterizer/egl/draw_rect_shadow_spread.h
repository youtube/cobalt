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
#include "cobalt/renderer/rasterizer/egl/draw_depth_stencil.h"

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
//   |   |   | (exclude scissor)   |   |   |
//   |   |   +---------------------+   |   |
//   |   |                             |   |
//   |   +-----------------------------+   |
//   | (include scissor)                   |
//   +-------------------------------------+
// When an offset is used for the shadow, the include and exclude scissors
// play a critical role.

// Handles drawing the solid "spread" portion of a box shadow. This also
// creates a stencil, using the depth buffer, for the pixels inside
// |include_scissor| excluding those in |exclude_scissor|.
class DrawRectShadowSpread : public DrawDepthStencil {
 public:
  // Fill the area between |inner_rect| and |outer_rect| with the specified
  // color.
  DrawRectShadowSpread(GraphicsState* graphics_state,
                       const BaseState& base_state,
                       const math::RectF& inner_rect,
                       const math::RectF& outer_rect,
                       const render_tree::ColorRGBA& color,
                       const math::RectF& include_scissor,
                       const math::RectF& exclude_scissor);

  void ExecuteOnscreenRasterize(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) OVERRIDE;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_SHADOW_SPREAD_H_
