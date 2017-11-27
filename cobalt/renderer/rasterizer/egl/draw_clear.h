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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_CLEAR_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_CLEAR_H_

#include "base/callback.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/renderer/rasterizer/egl/draw_object.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// This object handles clearing the current scissor area with the given color.
class DrawClear : public DrawObject {
 public:
  DrawClear(GraphicsState* graphics_state,
            const BaseState& base_state,
            const render_tree::ColorRGBA& clear_color);

  void ExecuteUpdateVertexBuffer(
      GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) override;
  void ExecuteRasterize(GraphicsState* graphics_state,
                        ShaderProgramManager* program_manager) override;
  base::TypeId GetTypeId() const override;

 private:
  render_tree::ColorRGBA clear_color_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_CLEAR_H_
