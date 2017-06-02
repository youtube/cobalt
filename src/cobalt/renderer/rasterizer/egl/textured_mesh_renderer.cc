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

#include "cobalt/renderer/rasterizer/egl/textured_mesh_renderer.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <string>
#include <vector>

#include "base/stringprintf.h"
#include "cobalt/math/size.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "third_party/glm/glm/gtc/type_ptr.hpp"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

TexturedMeshRenderer::TexturedMeshRenderer(
    backend::GraphicsContextEGL* graphics_context)
    : graphics_context_(graphics_context) {}

TexturedMeshRenderer::~TexturedMeshRenderer() {
  graphics_context_->MakeCurrent();
  if (quad_vbo_) {
    GL_CALL(glDeleteBuffers(1, &quad_vbo_.value()));
  }
  for (ProgramCache::iterator iter = blit_program_cache_.begin();
       iter != blit_program_cache_.end(); ++iter) {
    GL_CALL(glDeleteProgram(iter->second.gl_program_id));
  }
}

namespace {
void ConvertContentRegionToScaleTranslateVector(
    const math::Rect* content_region, const math::Size& texture_size,
    float* out_vec4) {
  SbMemorySet(out_vec4, 0, sizeof(float) * 4);
  if (!content_region) {
    // If no content region is provided, use the identity matrix.
    out_vec4[0] = 1.0f;
    out_vec4[1] = 1.0f;
    out_vec4[2] = 0.0f;
    out_vec4[3] = 0.0f;
    return;
  }

  float scale_x = (content_region->right() - content_region->x()) /
                  static_cast<float>(texture_size.width());
  float scale_y = (content_region->bottom() - content_region->y()) /
                  static_cast<float>(texture_size.height());
  float translate_x =
      content_region->x() / static_cast<float>(texture_size.width());
  float translate_y = (texture_size.height() - content_region->bottom()) /
                      static_cast<float>(texture_size.height());
  out_vec4[0] = scale_x;
  out_vec4[1] = scale_y;
  out_vec4[2] = translate_x;
  out_vec4[3] = translate_y;
}

// Used for RGB images.
const float kIdentityColorMatrix[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                        0.0f, 0.0f, 0.0f, 1.0f};

// Used for YUV images.
const float kBT709ColorMatrix[16] = {
    1.164f, 0.0f,   1.793f, -0.96925f, 1.164f, -0.213f, -0.533f, 0.30025f,
    1.164f, 2.112f, 0.0f,   -1.12875f, 0.0f,   0.0f,    0.0f,    1.0};

const float* GetColorMatrixForImageType(
    TexturedMeshRenderer::Image::Type type) {
  switch (type) {
    case TexturedMeshRenderer::Image::RGBA: {
      return kIdentityColorMatrix;
    } break;
    case TexturedMeshRenderer::Image::YUV_2PLANE_BT709:
    case TexturedMeshRenderer::Image::YUV_3PLANE_BT709: {
      return kBT709ColorMatrix;
    } break;
    default: { NOTREACHED(); }
  }
  return NULL;
}

}  // namespace

void TexturedMeshRenderer::RenderVBO(uint32 vbo, int num_vertices, uint32 mode,
                                     const Image& image,
                                     const glm::mat4& mvp_transform) {
  ProgramInfo blit_program =
      GetBlitProgram(image.type, image.textures[0].texture->GetTarget());

  GL_CALL(glUseProgram(blit_program.gl_program_id));

  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  GL_CALL(glVertexAttribPointer(kBlitPositionAttribute, 3, GL_FLOAT, GL_FALSE,
                                sizeof(float) * 5, 0));
  GL_CALL(glVertexAttribPointer(kBlitTexcoordAttribute, 2, GL_FLOAT, GL_FALSE,
                                sizeof(float) * 5,
                                reinterpret_cast<GLvoid*>(sizeof(float) * 3)));
  GL_CALL(glEnableVertexAttribArray(kBlitPositionAttribute));
  GL_CALL(glEnableVertexAttribArray(kBlitTexcoordAttribute));

  GL_CALL(glUniformMatrix4fv(blit_program.mvp_transform_uniform, 1, GL_FALSE,
                             glm::value_ptr(mvp_transform)));

  for (int i = 0; i < image.num_textures(); ++i) {
    DCHECK_EQ(image.textures[0].texture->GetTarget(),
              image.textures[i].texture->GetTarget());

    GL_CALL(glActiveTexture(GL_TEXTURE0 + i));
    GL_CALL(glBindTexture(image.textures[i].texture->GetTarget(),
                          image.textures[i].texture->gl_handle()));

    GL_CALL(glUniform1i(blit_program.texture_uniforms[i], i));

    float scale_translate_vector[4];
    ConvertContentRegionToScaleTranslateVector(
        &image.textures[i].content_region, image.textures[i].texture->GetSize(),
        scale_translate_vector);
    GL_CALL(glUniform4fv(blit_program.texcoord_scale_translate_uniforms[i], 1,
                         scale_translate_vector));
  }

  GL_CALL(glDrawArrays(mode, 0, num_vertices));

  for (int i = 0; i < image.num_textures(); ++i) {
    GL_CALL(glActiveTexture(GL_TEXTURE0 + i));
    GL_CALL(glBindTexture(image.textures[i].texture->GetTarget(), 0));
  }

  GL_CALL(glDisableVertexAttribArray(kBlitTexcoordAttribute));
  GL_CALL(glDisableVertexAttribArray(kBlitPositionAttribute));
  GL_CALL(glUseProgram(0));
}

void TexturedMeshRenderer::RenderQuad(const Image& image,
                                      const glm::mat4& mvp_transform) {
  RenderVBO(GetQuadVBO(), 4, GL_TRIANGLE_STRIP, image, mvp_transform);
}

uint32 TexturedMeshRenderer::GetQuadVBO() {
  if (!quad_vbo_) {
    // Setup a vertex buffer that can blit a quad with a full texture, to be
    // used by Frame::BlitToRenderTarget().
    struct QuadVertex {
      float position_x;
      float position_y;
      float position_z;
      float tex_coord_u;
      float tex_coord_v;
    };
    const QuadVertex kBlitQuadVerts[4] = {
        {-1.0f, -1.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, -1.0f, 0.0f, 1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 1.0, 0.0f, 1.0f, 1.0f},
    };

    quad_vbo_ = 0;
    GL_CALL(glGenBuffers(1, &quad_vbo_.value()));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, *quad_vbo_));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(kBlitQuadVerts),
                         kBlitQuadVerts, GL_STATIC_DRAW));
  }

  return *quad_vbo_;
}

// static
uint32 TexturedMeshRenderer::CreateVertexShader(
    const std::vector<TextureInfo>& textures) {
  uint32 blit_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  std::string blit_vertex_shader_source =
      "attribute vec3 a_position;"
      "attribute vec2 a_tex_coord;";
  for (unsigned int i = 0; i < textures.size(); ++i) {
    blit_vertex_shader_source +=
        StringPrintf("varying vec2 v_tex_coord_%s;", textures[i].name.c_str());
  }
  for (unsigned int i = 0; i < textures.size(); ++i) {
    blit_vertex_shader_source += StringPrintf(
        "uniform vec4 scale_translate_%s;", textures[i].name.c_str());
  }
  blit_vertex_shader_source +=
      "uniform mat4 model_view_projection_transform;"
      "void main() {"
      "  gl_Position = model_view_projection_transform * "
      "                    vec4(a_position.xyz, 1.0);";
  for (unsigned int i = 0; i < textures.size(); ++i) {
    const char* texture_name = textures[i].name.c_str();
    blit_vertex_shader_source += StringPrintf(
        "  v_tex_coord_%s = "
        "      a_tex_coord * scale_translate_%s.xy + scale_translate_%s.zw;",
        texture_name, texture_name, texture_name);
  }
  blit_vertex_shader_source += "}";

  int blit_vertex_shader_source_length = blit_vertex_shader_source.size();
  const char* blit_vertex_shader_source_c_str =
      blit_vertex_shader_source.c_str();
  GL_CALL(glShaderSource(blit_vertex_shader, 1,
                         &blit_vertex_shader_source_c_str,
                         &blit_vertex_shader_source_length));
  GL_CALL(glCompileShader(blit_vertex_shader));

  return blit_vertex_shader;
}

// static
uint32 TexturedMeshRenderer::CreateFragmentShader(
    uint32 texture_target, const std::vector<TextureInfo>& textures) {
  uint32 blit_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

  std::string sampler_type = "sampler2D";
  std::string blit_fragment_shader_source;

  if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
    sampler_type = "samplerExternalOES";
    blit_fragment_shader_source +=
        "#extension GL_OES_EGL_image_external : require\n";
  }

  blit_fragment_shader_source += "precision mediump float;";
  for (unsigned int i = 0; i < textures.size(); ++i) {
    blit_fragment_shader_source +=
        StringPrintf("varying vec2 v_tex_coord_%s;", textures[i].name.c_str());
  }
  for (unsigned int i = 0; i < textures.size(); ++i) {
    blit_fragment_shader_source +=
        StringPrintf("uniform %s texture_%s;", sampler_type.c_str(),
                     textures[i].name.c_str());
  }
  blit_fragment_shader_source +=
      "uniform mat4 to_rgb_color_matrix;"
      "void main() {"
      "  vec4 untransformed_color = vec4(";
  int components_used = 0;
  for (unsigned int i = 0; i < textures.size(); ++i) {
    if (i > 0) {
      blit_fragment_shader_source += ", ";
    }
    blit_fragment_shader_source += StringPrintf(
        "texture2D(texture_%s, v_tex_coord_%s).%s", textures[i].name.c_str(),
        textures[i].name.c_str(), textures[i].components.c_str());
    components_used += textures[i].components.length();
  }
  if (components_used == 3) {
    // Add an alpha component of 1.
    blit_fragment_shader_source += ", 1.0";
  }
  blit_fragment_shader_source +=
      ");"
      "  gl_FragColor = untransformed_color * to_rgb_color_matrix;"
      "}";

  int blit_fragment_shader_source_length = blit_fragment_shader_source.size();
  const char* blit_fragment_shader_source_c_str =
      blit_fragment_shader_source.c_str();
  GL_CALL(glShaderSource(blit_fragment_shader, 1,
                         &blit_fragment_shader_source_c_str,
                         &blit_fragment_shader_source_length));
  GL_CALL(glCompileShader(blit_fragment_shader));

  return blit_fragment_shader;
}

// static
TexturedMeshRenderer::ProgramInfo TexturedMeshRenderer::MakeBlitProgram(
    const float* color_matrix, uint32 texture_target,
    const std::vector<TextureInfo>& textures) {
  int total_components = 0;
  for (unsigned int i = 0; i < textures.size(); ++i) {
    total_components += textures[i].components.length();
  }
  DCHECK(total_components == 3 || total_components == 4);

  ProgramInfo result;

  // Create the blit program.
  // Setup shaders used when blitting the current texture.
  result.gl_program_id = glCreateProgram();

  uint32 blit_vertex_shader = CreateVertexShader(textures);
  GL_CALL(glAttachShader(result.gl_program_id, blit_vertex_shader));

  uint32 blit_fragment_shader = CreateFragmentShader(texture_target, textures);
  GL_CALL(glAttachShader(result.gl_program_id, blit_fragment_shader));

  GL_CALL(glBindAttribLocation(result.gl_program_id, kBlitPositionAttribute,
                               "a_position"));
  GL_CALL(glBindAttribLocation(result.gl_program_id, kBlitTexcoordAttribute,
                               "a_tex_coord"));

  GL_CALL(glLinkProgram(result.gl_program_id));

  result.mvp_transform_uniform = glGetUniformLocation(
      result.gl_program_id, "model_view_projection_transform");
  for (unsigned int i = 0; i < textures.size(); ++i) {
    std::string scale_translate_uniform_name =
        StringPrintf("scale_translate_%s", textures[i].name.c_str());
    result.texcoord_scale_translate_uniforms[i] = glGetUniformLocation(
        result.gl_program_id, scale_translate_uniform_name.c_str());

    std::string texture_uniform_name =
        StringPrintf("texture_%s", textures[i].name.c_str());
    result.texture_uniforms[i] = glGetUniformLocation(
        result.gl_program_id, texture_uniform_name.c_str());
  }

  // Upload the color matrix right away since it won't change from draw to draw.
  GL_CALL(glUseProgram(result.gl_program_id));
  uint32 to_rgb_color_matrix_uniform =
      glGetUniformLocation(result.gl_program_id, "to_rgb_color_matrix");
  GL_CALL(glUniformMatrix4fv(to_rgb_color_matrix_uniform, 1, GL_FALSE,
                             color_matrix));
  GL_CALL(glUseProgram(0));

  GL_CALL(glDeleteShader(blit_fragment_shader));
  GL_CALL(glDeleteShader(blit_vertex_shader));

  return result;
}

TexturedMeshRenderer::ProgramInfo TexturedMeshRenderer::GetBlitProgram(
    Image::Type type, uint32 texture_target) {
  CacheKey key(texture_target, type);

  ProgramCache::iterator found = blit_program_cache_.find(key);
  if (found == blit_program_cache_.end()) {
    const float* color_matrix = GetColorMatrixForImageType(type);
    ProgramInfo result;
    switch (type) {
      case Image::RGBA: {
        std::vector<TextureInfo> texture_infos;
        texture_infos.push_back(TextureInfo("rgba", "rgba"));
        result = MakeBlitProgram(color_matrix, texture_target, texture_infos);
      } break;
      case Image::YUV_2PLANE_BT709: {
        std::vector<TextureInfo> texture_infos;
        texture_infos.push_back(TextureInfo("y", "a"));
        texture_infos.push_back(TextureInfo("uv", "ba"));
        result = MakeBlitProgram(color_matrix, texture_target, texture_infos);
      } break;
      case Image::YUV_3PLANE_BT709: {
        std::vector<TextureInfo> texture_infos;
        texture_infos.push_back(TextureInfo("y", "a"));
        texture_infos.push_back(TextureInfo("u", "a"));
        texture_infos.push_back(TextureInfo("v", "a"));
        result = MakeBlitProgram(color_matrix, texture_target, texture_infos);
      } break;
      default: { NOTREACHED(); }
    }

    // Save our shader into the cache.
    found = blit_program_cache_.insert(std::make_pair(key, result)).first;
  }

  return found->second;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
