// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_RRECT_COLOR_TEXTURE_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_RRECT_COLOR_TEXTURE_H_

#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/rasterizer/egl/draw_object.h"
#include "egl/generated_shader_impl.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Handles drawing a textured rounded rectangle modulated by a given color.
class DrawRRectColorTexture : public DrawObject {
 public:
  DrawRRectColorTexture(GraphicsState* graphics_state,
                        const BaseState& base_state, const math::RectF& rect,
                        const render_tree::ColorRGBA& color,
                        const backend::TextureEGL* texture,
                        const math::Matrix3F& texcoord_transform,
                        bool clamp_texcoords);
  DrawRRectColorTexture(GraphicsState* graphics_state,
                        const BaseState& base_state, const math::RectF& rect,
                        const render_tree::ColorRGBA& color,
                        const backend::TextureEGL* y_texture,
                        const backend::TextureEGL* u_texture,
                        const backend::TextureEGL* v_texture,
                        const float (&color_transform_in_column_major)[16],
                        const math::Matrix3F& texcoord_transform,
                        bool clamp_texcoords);

  void ExecuteUpdateVertexBuffer(
      GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) override;
  void ExecuteRasterize(GraphicsState* graphics_state,
                        ShaderProgramManager* program_manager) override;
  base::TypeId GetTypeId() const override;

 private:
  static const int kMaxNumOfTextures =
      render_tree::MultiPlaneImageDataDescriptor::kMaxPlanes;

  struct VertexAttributes {
    float position[2];
    float texcoord[2];
    RCorner rcorner;
  };

  void SetupVertexShader(GraphicsState* graphics_state,
                         const ShaderVertexRcornerTexcoord& vertex_shader);
  template <typename FragmentShader>
  void SetupFragmentShaderAndDraw(GraphicsState* graphics_state,
                                  const FragmentShader& fragment_shader);

  const math::Matrix3F texcoord_transform_;
  float color_transform_[16];
  math::RectF rect_;
  const render_tree::ColorRGBA color_;
  const backend::TextureEGL* textures_[kMaxNumOfTextures];

  uint8_t* vertex_buffer_;
  // texcoord clamping (min u, min v, max u, max v)
  float texcoord_clamps_[kMaxNumOfTextures][4];
  const bool clamp_texcoords_;
  bool tile_texture_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_RRECT_COLOR_TEXTURE_H_
