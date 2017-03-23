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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_TEXTURE_H_
#define COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_TEXTURE_H_

#include "base/callback.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/rasterizer/egl/draw_object.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Handles drawing a textured rectangle.
class DrawRectTexture : public DrawObject {
 public:
  typedef base::Callback<void(const backend::TextureEGL** out_texture,
                              math::Matrix3F* out_texcoord_transform)>
      GenerateTextureFunction;

  DrawRectTexture(GraphicsState* graphics_state,
                  const BaseState& base_state,
                  const math::RectF& rect,
                  const backend::TextureEGL* texture,
                  const math::Matrix3F& texcoord_transform);

  DrawRectTexture(GraphicsState* graphics_state,
                  const BaseState& base_state,
                  const math::RectF& rect,
                  const GenerateTextureFunction& generate_texture);

  void ExecuteOffscreenRasterize(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) OVERRIDE;
  void ExecuteOnscreenUpdateVertexBuffer(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) OVERRIDE;
  void ExecuteOnscreenRasterize(GraphicsState* graphics_state,
      ShaderProgramManager* program_manager) OVERRIDE;

 private:
  math::Matrix3F texcoord_transform_;
  math::RectF rect_;
  const backend::TextureEGL* texture_;
  GenerateTextureFunction generate_texture_;

  uint8_t* vertex_buffer_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_DRAW_RECT_TEXTURE_H_
