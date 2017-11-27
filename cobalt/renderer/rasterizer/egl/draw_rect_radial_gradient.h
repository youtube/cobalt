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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_RADIAL_GRADIENT_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_RADIAL_GRADIENT_H_

#include <vector>

#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/rasterizer/egl/draw_object.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Handles drawing a rectangle with a radial color gradient.
class DrawRectRadialGradient : public DrawObject {
 public:
  DrawRectRadialGradient(GraphicsState* graphics_state,
                         const BaseState& base_state,
                         const math::RectF& rect,
                         const render_tree::RadialGradientBrush& brush,
                         const GetScratchTextureFunction& get_scratch_texture);

  void ExecuteUpdateVertexBuffer(
      GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) override;
  void ExecuteRasterize(GraphicsState* graphics_state,
                        ShaderProgramManager* program_manager) override;
  base::TypeId GetTypeId() const override;

  bool IsValid() const { return lookup_texture_ != nullptr; }

 private:
  struct VertexAttributes {
    float position[2];
    float offset[2];
  };

  void InitializeLookupTexture(const render_tree::RadialGradientBrush& brush);
  void AddVertex(float x, float y, const math::PointF& offset_center,
      const math::PointF& offset_scale);

  std::vector<VertexAttributes> attributes_;
  const backend::TextureEGL* lookup_texture_;
  math::RectF lookup_region_;
  uint8_t* vertex_buffer_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_RADIAL_GRADIENT_H_
