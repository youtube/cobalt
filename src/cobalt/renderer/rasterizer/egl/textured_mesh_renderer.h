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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_TEXTURED_MESH_RENDERER_H_
#define COBALT_RENDERER_RASTERIZER_EGL_TEXTURED_MESH_RENDERER_H_

#include <map>

#include "base/optional.h"
#include "cobalt/math/rect.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/texture.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// Helper class to render textured meshes.  The class is centered around the
// public method RenderVBO(), which can be used to render an arbitrary GLES2
// vertex buffer object (where each vertex has a 3-float position and a
// 2-float texture coordinate), with a specified texture applied to it.
class TexturedMeshRenderer {
 public:
  explicit TexturedMeshRenderer(backend::GraphicsContextEGL* graphics_context);
  ~TexturedMeshRenderer();

  // A context must be current before this method is called.  Before calling
  // this function, please configure glViewport() and glScissor() as desired.
  // The content region indicates which source rectangle of the input texture
  // should be used (i.e. the VBO's texture coordinates will be transformed to
  // lie within this rectangle).
  void RenderVBO(uint32 vbo, int num_vertices, uint32 mode,
                 const backend::TextureEGL* texture,
                 const math::Rect& content_region);

  // This method will call into RenderVBO(), so the comments pertaining to that
  // method also apply to this method.  This method renders with vertex
  // positions (-1, -1) -> (1, 1), so it will occupy the entire viewport
  // specified by glViewport().
  void RenderQuad(const backend::TextureEGL* texture,
                  const math::Rect& content_region);

 private:
  uint32 GetQuadVBO();
  uint32 GetBlitProgram(uint32 texture_target);

  backend::GraphicsContextEGL* graphics_context_;

  // Since different texture targets can warrant different shaders/programs,
  // we keep a map of blit programs for each texture target, and initialize
  // them lazily.
  std::map<uint32, uint32> blit_program_per_texture_target_;

  static const int kBlitPositionAttribute = 0;
  static const int kBlitTexcoordAttribute = 1;

  base::optional<uint32> quad_vbo_;

  uint32 texcoord_scale_translate_uniform_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_TEXTURED_MESH_RENDERER_H_
