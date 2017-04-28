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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_DEPTH_STENCIL_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_DEPTH_STENCIL_H_

#include <vector>

#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/renderer/rasterizer/egl/draw_poly_color.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Handles creation of a depth stencil. Pixels within the include scissor will
// have their depth set to base_state.depth. Pixels within the exclude scissor,
// if specified, will have their depth set to the next closest depth value. An
// |include_scissor| must always be specified.
//
// NOTE: This object leaves the graphics state modified so that only the
// stencilled pixels are affected by later draw calls. Use UndoStencilState
// once finished with the stencil.
// NOTE: If an |exclude_scissor| is specified, then this object uses two depth
// values -- the incoming value in base_state.depth and the next closest.
// NOTE: Since scissor rects pollute the depth buffer, they should only be
// used during the tranparency pass because subsequent draws are guaranteed to
// be above (or not overlap) these pixels.
class DrawDepthStencil : public DrawPolyColor {
 public:
  DrawDepthStencil(GraphicsState* graphics_state,
                   const BaseState& base_state,
                   const math::RectF& include_scissor,
                   const math::RectF& exclude_scissor);

  void ExecuteOnscreenRasterize(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) OVERRIDE;

  // ExecuteOnscreenRasterize / DrawDepthStencil change the graphics state
  // so that subsequent draw calls affect only the stencilled pixels. This
  // function reverts the graphics state back to normal.
  void UndoStencilState(GraphicsState* graphics_state);

 protected:
  explicit DrawDepthStencil(const BaseState& base_state);
  void AddStencil(const math::RectF& include_scissor,
                  const math::RectF& exclude_scissor);
  void DrawStencil(GraphicsState* graphics_state);
  static size_t MaxVertsNeededForStencil() { return 8; }

  GLint include_scissor_first_vert_;
  GLint exclude_scissor_first_vert_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_DEPTH_STENCIL_H_
