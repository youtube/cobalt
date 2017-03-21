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

#include "cobalt/math/size.h"
#include "cobalt/renderer/backend/egl/utils.h"

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
  for (std::map<uint32, uint32>::iterator iter =
           blit_program_per_texture_target_.begin();
       iter != blit_program_per_texture_target_.end(); ++iter) {
    GL_CALL(glDeleteProgram(iter->second));
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
}  // namespace

void TexturedMeshRenderer::RenderVBO(uint32 vbo, int num_vertices, uint32 mode,
                                     const backend::TextureEGL* texture,
                                     const math::Rect& content_region) {
  uint32 blit_program = GetBlitProgram(texture->GetTarget());

  GL_CALL(glUseProgram(blit_program));

  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  GL_CALL(glVertexAttribPointer(kBlitPositionAttribute, 2, GL_FLOAT, GL_FALSE,
                                sizeof(float) * 4, 0));
  GL_CALL(glVertexAttribPointer(kBlitTexcoordAttribute, 2, GL_FLOAT, GL_FALSE,
                                sizeof(float) * 4,
                                reinterpret_cast<GLvoid*>(sizeof(float) * 2)));
  GL_CALL(glEnableVertexAttribArray(kBlitPositionAttribute));
  GL_CALL(glEnableVertexAttribArray(kBlitTexcoordAttribute));

  GL_CALL(glActiveTexture(GL_TEXTURE0));
  GL_CALL(glBindTexture(texture->GetTarget(), texture->gl_handle()));

  float scale_translate_vector[4];
  ConvertContentRegionToScaleTranslateVector(
      &content_region, texture->GetSize(), scale_translate_vector);
  GL_CALL(glUniform4fv(texcoord_scale_translate_uniform_, 1,
                       scale_translate_vector));

  GL_CALL(glDrawArrays(mode, 0, num_vertices));

  GL_CALL(glBindTexture(texture->GetTarget(), 0));

  GL_CALL(glDisableVertexAttribArray(kBlitTexcoordAttribute));
  GL_CALL(glDisableVertexAttribArray(kBlitPositionAttribute));
  GL_CALL(glUseProgram(0));
}

void TexturedMeshRenderer::RenderQuad(const backend::TextureEGL* texture,
                                      const math::Rect& content_region) {
  RenderVBO(GetQuadVBO(), 4, GL_TRIANGLE_STRIP, texture, content_region);
}

uint32 TexturedMeshRenderer::GetQuadVBO() {
  if (!quad_vbo_) {
    // Setup a vertex buffer that can blit a quad with a full texture, to be
    // used by Frame::BlitToRenderTarget().
    struct QuadVertex {
      float position_x;
      float position_y;
      float tex_coord_u;
      float tex_coord_v;
    };
    const QuadVertex kBlitQuadVerts[4] = {
        {-1.0f, -1.0f, 0.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f, 1.0f},
        {1.0f, -1.0f, 1.0f, 0.0f},
        {1.0f, 1.0, 1.0f, 1.0f},
    };

    quad_vbo_ = 0;
    GL_CALL(glGenBuffers(1, &quad_vbo_.value()));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, *quad_vbo_));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(kBlitQuadVerts),
                         kBlitQuadVerts, GL_STATIC_DRAW));
  }

  return *quad_vbo_;
}

uint32 TexturedMeshRenderer::GetBlitProgram(uint32 texture_target) {
  std::map<uint32, uint32>::iterator found =
      blit_program_per_texture_target_.find(texture_target);
  if (found == blit_program_per_texture_target_.end()) {
    // Create the blit program.
    // Setup shaders used when blitting the current texture.
    uint32 blit_program = glCreateProgram();

    uint32 blit_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    const char* blit_vertex_shader_source =
        "attribute vec2 a_position;"
        "attribute vec2 a_tex_coord;"
        "varying vec2 v_tex_coord;"
        "uniform vec4 scale_translate;"
        "void main() {"
        "  gl_Position = vec4(a_position.x, a_position.y, 0, 1);"
        "  v_tex_coord = "
        "      a_tex_coord * scale_translate.xy + scale_translate.zw;"
        "}";
    int blit_vertex_shader_source_length = strlen(blit_vertex_shader_source);
    GL_CALL(glShaderSource(blit_vertex_shader, 1, &blit_vertex_shader_source,
                           &blit_vertex_shader_source_length));
    GL_CALL(glCompileShader(blit_vertex_shader));

    char buffer[2048];
    int length;
    glGetShaderInfoLog(blit_vertex_shader, 2048, &length, buffer);
    DLOG(INFO) << "vertex shader: " << buffer;

    GL_CALL(glAttachShader(blit_program, blit_vertex_shader));

    std::string sampler_type = "sampler";
    if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
      sampler_type = "samplerExternalOES";
    }
    uint32 blit_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    std::string blit_fragment_shader_source =
        "precision mediump float;"
        "varying vec2 v_tex_coord;"
        "uniform " +
        sampler_type +
        " texture;"
        "void main() {"
        "  gl_FragColor = texture2D(texture, v_tex_coord);"
        "}";
    if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
      blit_fragment_shader_source =
          "#extension GL_OES_EGL_image_external : require\n" +
          blit_fragment_shader_source;
    }

    int blit_fragment_shader_source_length = blit_fragment_shader_source.size();
    const char* blit_fragment_shader_source_c_str =
        blit_fragment_shader_source.c_str();
    GL_CALL(glShaderSource(blit_fragment_shader, 1,
                           &blit_fragment_shader_source_c_str,
                           &blit_fragment_shader_source_length));
    GL_CALL(glCompileShader(blit_fragment_shader));

    glGetShaderInfoLog(blit_fragment_shader, 2048, &length, buffer);
    DLOG(INFO) << "fragment shader: " << buffer;

    GL_CALL(glAttachShader(blit_program, blit_fragment_shader));

    GL_CALL(glBindAttribLocation(blit_program, kBlitPositionAttribute,
                                 "a_position"));
    GL_CALL(glBindAttribLocation(blit_program, kBlitTexcoordAttribute,
                                 "a_tex_coord"));

    GL_CALL(glLinkProgram(blit_program));

    glGetProgramInfoLog(blit_program, 2048, &length, buffer);
    DLOG(INFO) << buffer;

    texcoord_scale_translate_uniform_ =
        glGetUniformLocation(blit_program, "scale_translate");

    GL_CALL(glDeleteShader(blit_fragment_shader));
    GL_CALL(glDeleteShader(blit_vertex_shader));

    // Save our shader into the cache.
    found =
        blit_program_per_texture_target_.insert(std::make_pair(texture_target,
                                                               blit_program))
            .first;
  }

  return found->second;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
