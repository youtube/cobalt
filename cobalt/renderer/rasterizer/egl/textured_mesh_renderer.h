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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_TEXTURED_MESH_RENDERER_H_
#define COBALT_RENDERER_RASTERIZER_EGL_TEXTURED_MESH_RENDERER_H_

#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "cobalt/math/rect.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "third_party/glm/glm/mat4x4.hpp"

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

  // The Image structure acts as an interface for describing an image, which
  // in the case of YUV formats, may be composed of multiple textures.
  struct Image {
    enum Type {
      // YUV BT709 image, where the Y component is on one plane and the UV
      // components are on a second plane.  The valid range of individual color
      // component is [16, 235].  NV12 would for example choose this format
      // type.
      YUV_2PLANE_BT709,
      // YUV BT601 image where the Y, U and V components are all on different
      // textures.  The valid range of individual color component is [0, 255].
      YUV_3PLANE_BT601_FULL_RANGE,
      // YUV BT709 image where the Y, U and V components are all on different
      // textures.  The valid range of individual color component is [16, 235].
      YUV_3PLANE_BT709,
      // YUV BT2020 image where the Y, U and V components are all on different
      // 10bit unnormalized textures.
      YUV_3PLANE_10BIT_BT2020,
      // YUV BT2020 image where the Y, U and V components are all on different
      // textures. Its pixels are stored in 32 bits, and each pixel represents
      // three 10-bit pixels with two bits of gap.
      YUV_3PLANE_10BIT_COMPACT_BT2020,
      // 1 texture is used that contains RGBA pixels.
      RGBA,
      // 1 texture plane is used where Y is sampled twice for each UV sample
      // (horizontally).
      YUV_UYVY_422_BT709,
    };

    struct Texture {
      const backend::TextureEGL* texture;
      math::RectF content_region;
    };

    // Returns the number of valid textures in this image, based on its format.
    int num_textures() const {
      switch (type) {
        case YUV_2PLANE_BT709:
          return 2;
        case YUV_3PLANE_BT601_FULL_RANGE:
        case YUV_3PLANE_BT709:
        case YUV_3PLANE_10BIT_BT2020:
        case YUV_3PLANE_10BIT_COMPACT_BT2020:
          return 3;
        case RGBA:
        case YUV_UYVY_422_BT709:
          return 1;
        default:
          NOTREACHED();
      }
      return 0;
    }

    Type type;
    bool is_hlg;
    Texture textures[3];
  };

  // A context must be current before this method is called.  Before calling
  // this function, please configure glViewport() and glScissor() as desired.
  // The content region indicates which source rectangle of the input texture
  // should be used (i.e. the VBO's texture coordinates will be transformed to
  // lie within this rectangle).
  void RenderVBO(uint32 vbo, int num_vertices, uint32 mode, const Image& image,
                 const glm::mat4& mvp_transform);

  // This method will call into RenderVBO(), so the comments pertaining to that
  // method also apply to this method.  This method renders with vertex
  // positions (-1, -1) -> (1, 1), so it will occupy the entire viewport
  // specified by glViewport().
  void RenderQuad(const Image& image, const glm::mat4& mvp_transform);

 private:
  struct TextureInfo {
    TextureInfo(const std::string& name, const std::string& components)
        : name(name), components(components) {}

    std::string name;
    std::string components;
  };
  struct ProgramInfo {
    int32 mvp_transform_uniform;
    int32 texcoord_scale_translate_uniforms[3];
    int32 texture_uniforms[3];
    int32 texture_size_uniforms[3];
    uint32 gl_program_id;
    int32 viewport_size_uniform;
    int32 enable_filtering_uniform;
    int32 content_region_size_uniform;
    int32 texture_size_as_unpacked_uniform;
    int32 subtexture_size_uniform;
  };
  // We key each program off of their GL texture type and image type.
  typedef std::tuple<uint32, Image::Type, base::Optional<int32> > CacheKey;
  typedef std::map<CacheKey, ProgramInfo> ProgramCache;

  uint32 GetQuadVBO();
  ProgramInfo GetBlitProgram(const Image& image);

  static ProgramInfo MakeBlitProgram(const float* color_matrix,
                                     const std::vector<TextureInfo>& textures,
                                     uint32 blit_fragment_shader);

  static uint32 CreateFragmentShader(uint32 texture_target,
                                     const std::vector<TextureInfo>& textures,
                                     const float* color_matrix,
                                     bool hlg = false);
  static uint32 CreateVertexShader(const std::vector<TextureInfo>& textures);

  // UYVY textures need a special fragment shader to handle the unique aspect
  // of applying bilinear filtering within a texel between the two Y values.
  static uint32 CreateUYVYFragmentShader(uint32 texture_target,
                                         int32 texture_wrap_s);
  // YUV compacted textures need a special fragment shader to handle compacted
  // pixels. Compacted textures support YUV420 only.
  static uint32 CreateYUVCompactedTexturesFragmentShader(uint32 texture_target,
                                                         bool hlg = false);

  backend::GraphicsContextEGL* graphics_context_;

  // Since different texture targets can warrant different shaders/programs,
  // we keep a map of blit programs for each texture target, and initialize
  // them lazily.
  ProgramCache blit_program_cache_;

  static const int kBlitPositionAttribute = 0;
  static const int kBlitTexcoordAttribute = 1;

  base::Optional<uint32> quad_vbo_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_TEXTURED_MESH_RENDERER_H_
