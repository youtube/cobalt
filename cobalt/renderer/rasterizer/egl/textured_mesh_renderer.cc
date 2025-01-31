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

#include "cobalt/renderer/rasterizer/egl/textured_mesh_renderer.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "cobalt/math/size.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "starboard/configuration.h"
#include "starboard/extension/graphics.h"
#include "third_party/angle/include/angle_hdr.h"
#include "third_party/glm/glm/gtc/type_ptr.hpp"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

TexturedMeshRenderer::TexturedMeshRenderer(
    backend::GraphicsContextEGL* graphics_context)
    : graphics_context_(graphics_context) {}

TexturedMeshRenderer::~TexturedMeshRenderer() {
  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_);
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
    const math::RectF* content_region, const math::Size& texture_size,
    float* out_vec4) {
  if (!content_region) {
    // If no content region is provided, use the identity matrix.
    out_vec4[0] = 1.0f;
    out_vec4[1] = 1.0f;
    out_vec4[2] = 0.0f;
    out_vec4[3] = 0.0f;
    return;
  }

  float scale_x =
      content_region->width() / static_cast<float>(texture_size.width());
  float scale_y =
      content_region->height() / static_cast<float>(texture_size.height());
  float translate_x =
      content_region->x() / static_cast<float>(texture_size.width());
  float translate_y = (texture_size.height() - content_region->bottom()) /
                      static_cast<float>(texture_size.height());
  out_vec4[0] = scale_x;
  out_vec4[1] = scale_y;
  out_vec4[2] = translate_x;
  out_vec4[3] = translate_y;
}

// Used for YUV images.
const float kBT601FullRangeColorMatrix[16] = {
    1.0f, 0.0f,   1.402f, -0.701,  1.0f, -0.34414f, -0.71414f, 0.529f,
    1.0f, 1.772f, 0.0f,   -0.886f, 0.0f, 0.0f,      0.0f,      1.f};

// Used for YUV images.
const float kBT709ColorMatrix[16] = {
    1.164f, 0.0f,   1.793f, -0.96925f, 1.164f, -0.213f, -0.533f, 0.30025f,
    1.164f, 2.112f, 0.0f,   -1.12875f, 0.0f,   0.0f,    0.0f,    1.0f};

// Used for 10bit unnormalized YUV images.
// Y is between 64 and 940 inclusive. U and V are between 64 and 960 inclusive.
// it is 1023/(940-64) = 1.1678 for Y and 1023/(960-64) = 1.1417 for U and V.
// 64 is the scale factor for 10 bit.
// Input YUV must be subtracted by (0.0625, 0.5, 0.5), so
// -1.1678 * 0.0625  - 0 * 0.5         - 1.6835 * 0.5    = -0.9147
// -1.1678 * 0.0625  -(-0.1878 * 0.5)  - (-0.6522 * 0.5) = 0.347
// -1.1678 * 0.0625  - (2.1479f * 0.5) - 0 * 0.5         = -1.1469
const float k10BitBT2020ColorMatrix[16] = {
    64 * 1.1678f, 0.0f,          64 * 1.6835f,  -0.9147f,
    64 * 1.1678f, 64 * -0.1878f, 64 * -0.6522f, 0.347f,
    64 * 1.1678f, 64 * 2.1479f,  0.0f,          -1.1469f,
    0.0f,         0.0f,          0.0f,          1.0f};

const float k10BitBT2020ColorMatrixCompacted[16] = {
    1.1678f, 0.0f,    1.6835f, -0.9147f, 1.1678f, -0.1878f, -0.6522f, 0.347f,
    1.1678f, 2.1479f, 0.0f,    -1.1469f, 0.0f,    0.0f,     0.0f,     1.0f};

const float* GetColorMatrixForImageType(
    TexturedMeshRenderer::Image::Type type) {
  switch (type) {
    case TexturedMeshRenderer::Image::RGBA: {
      // No color matrix needed for RGBA to RGBA.
      return nullptr;
    } break;
    case TexturedMeshRenderer::Image::YUV_3PLANE_BT601_FULL_RANGE: {
      return kBT601FullRangeColorMatrix;
    } break;
    case TexturedMeshRenderer::Image::YUV_2PLANE_BT709:
    case TexturedMeshRenderer::Image::YUV_3PLANE_BT709:
    case TexturedMeshRenderer::Image::YUV_UYVY_422_BT709: {
      return kBT709ColorMatrix;
    } break;
    case TexturedMeshRenderer::Image::YUV_3PLANE_10BIT_COMPACT_BT2020: {
      return k10BitBT2020ColorMatrixCompacted;
    } break;
    case TexturedMeshRenderer::Image::YUV_3PLANE_10BIT_BT2020: {
      return k10BitBT2020ColorMatrix;
    } break;
    default: {
      NOTREACHED();
    }
  }
  return NULL;
}

}  // namespace

void TexturedMeshRenderer::RenderVBO(uint32 vbo, int num_vertices, uint32 mode,
                                     const Image& image,
                                     const glm::mat4& mvp_transform) {
  ProgramInfo blit_program = GetBlitProgram(image);

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
    if (image.type == Image::YUV_UYVY_422_BT709) {
      // For UYVY, wrap mode is handled within the fragment shader, ensure here
      // that it is always set to GL_REPEAT.
      GL_CALL(glTexParameteri(image.textures[0].texture->GetTarget(),
                              GL_TEXTURE_WRAP_S, GL_REPEAT));
    }

    if (image.type == Image::YUV_3PLANE_10BIT_COMPACT_BT2020) {
      GL_CALL(
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
      GL_CALL(
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    }

    GL_CALL(glUniform1i(blit_program.texture_uniforms[i], i));

    if (blit_program.texture_size_uniforms[i] != -1) {
      math::Size texture_size = image.textures[i].texture->GetSize();
      int texture_sizes[2] = {texture_size.width(), texture_size.height()};
      GL_CALL(glUniform2iv(blit_program.texture_size_uniforms[i], 1,
                           texture_sizes));
    }

    math::Size content_size = image.textures[i].texture->GetSize();

    float scale_translate_vector[4];
    ConvertContentRegionToScaleTranslateVector(
        &image.textures[i].content_region, content_size,
        scale_translate_vector);
    GL_CALL(glUniform4fv(blit_program.texcoord_scale_translate_uniforms[i], 1,
                         scale_translate_vector));
  }

  if (image.type == Image::YUV_3PLANE_10BIT_COMPACT_BT2020) {
    math::Size viewport_size = graphics_context_->GetWindowSize();
    GL_CALL(glUniform2f(blit_program.viewport_size_uniform,
                        viewport_size.width(), viewport_size.height()));

    SB_DCHECK(image.num_textures() >= 1);
    math::Size texture_size = image.textures[0].texture->GetSize();
    // The pixel shader requires the actual frame size of the first plane
    // for calculations.
    size_t subtexture_width = (texture_size.width() + 2) / 3;
    GL_CALL(glUniform2f(blit_program.subtexture_size_uniform, subtexture_width,
                        texture_size.height()));

    const int kCompactTextureFilteringThreshold = 1920;
    int enable_filtering = (abs(image.textures[0].content_region.width()) <
                            kCompactTextureFilteringThreshold)
                               ? 1
                               : 0;
    GL_CALL(
        glUniform1i(blit_program.enable_filtering_uniform, enable_filtering));

    GL_CALL(glUniform2f(blit_program.content_region_size_uniform,
                        abs(image.textures[0].content_region.width()),
                        abs(image.textures[0].content_region.height())));

    GL_CALL(glUniform2f(blit_program.texture_size_as_unpacked_uniform,
                        texture_size.width(), texture_size.height()));
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
  RenderVBO(GetQuadVBO(), 6, GL_TRIANGLES, image, mvp_transform);
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
    const QuadVertex kBlitQuadVerts[6] = {
        {-1.0f, -1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, -1.0f, 0.0f, 1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f, 0.0f, 1.0f},  {-1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, -1.0f, 0.0f, 1.0f, 0.0f},  {1.0f, 1.0, 0.0f, 1.0f, 1.0f},
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
  uint32 blit_vertex_shader = GL_CALL_SIMPLE(glCreateShader(GL_VERTEX_SHADER));
  std::string blit_vertex_shader_source =
      "attribute vec3 a_position;"
      "attribute vec2 a_tex_coord;";
  for (unsigned int i = 0; i < textures.size(); ++i) {
    blit_vertex_shader_source += base::StringPrintf(
        "varying vec2 v_tex_coord_%s;", textures[i].name.c_str());
  }
  for (unsigned int i = 0; i < textures.size(); ++i) {
    blit_vertex_shader_source += base::StringPrintf(
        "uniform vec4 scale_translate_%s;", textures[i].name.c_str());
  }
  blit_vertex_shader_source +=
      "uniform mat4 model_view_projection_transform;"
      "void main() {"
      "  gl_Position = model_view_projection_transform * "
      "                    vec4(a_position.xyz, 1.0);";
  for (unsigned int i = 0; i < textures.size(); ++i) {
    const char* texture_name = textures[i].name.c_str();
    blit_vertex_shader_source += base::StringPrintf(
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

namespace {

uint32 CompileShader(const std::string& shader_source) {
  uint32 blit_fragment_shader =
      GL_CALL_SIMPLE(glCreateShader(GL_FRAGMENT_SHADER));

  int shader_source_length = shader_source.size();
  const char* shader_source_c_str = shader_source.c_str();
  GL_CALL(glShaderSource(blit_fragment_shader, 1, &shader_source_c_str,
                         &shader_source_length));
  GL_CALL(glCompileShader(blit_fragment_shader));

  GLint is_compiled = GL_FALSE;
  GL_CALL(glGetShaderiv(blit_fragment_shader, GL_COMPILE_STATUS, &is_compiled));
  if (is_compiled != GL_TRUE) {
    const GLsizei kMaxLogLength = 2048;
    GLsizei log_length = 0;
    GLchar log[kMaxLogLength];
    GL_CALL_SIMPLE(glGetShaderInfoLog(blit_fragment_shader, kMaxLogLength,
                                      &log_length, log));
    DLOG(ERROR) << "shader error: " << log;
    DLOG(ERROR) << "shader source:\n" << shader_source;
  }
  CHECK_EQ(is_compiled, GL_TRUE);

  return blit_fragment_shader;
}

struct SamplerInfo {
  std::string type;
  std::string preamble;
};

SamplerInfo GetSamplerInfo(uint32 texture_target) {
  SamplerInfo sampler_info;
  sampler_info.type = "sampler2D";

  if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
    sampler_info.type = "samplerExternalOES";
    sampler_info.preamble = "#extension GL_OES_EGL_image_external : require\n";
  }

  return sampler_info;
}
}  // namespace

// static
uint32 TexturedMeshRenderer::CreateFragmentShader(
    uint32 texture_target, const std::vector<TextureInfo>& textures,
    const float* color_matrix, bool tonemapping) {
  SamplerInfo sampler_info = GetSamplerInfo(texture_target);

  std::string blit_fragment_shader_source = sampler_info.preamble;

  blit_fragment_shader_source += "precision mediump float;";
  for (unsigned int i = 0; i < textures.size(); ++i) {
    blit_fragment_shader_source += base::StringPrintf(
        "varying vec2 v_tex_coord_%s;", textures[i].name.c_str());
  }
  for (unsigned int i = 0; i < textures.size(); ++i) {
    blit_fragment_shader_source +=
        base::StringPrintf("uniform %s texture_%s;", sampler_info.type.c_str(),
                           textures[i].name.c_str());
  }
  if (color_matrix) {
    blit_fragment_shader_source += "uniform mat4 to_rgb_color_matrix;";
  }
  blit_fragment_shader_source +=
      "void main() {"
      "  vec4 untransformed_color = vec4(";
  int components_used = 0;
  for (unsigned int i = 0; i < textures.size(); ++i) {
    if (i > 0) {
      blit_fragment_shader_source += ", ";
    }
    blit_fragment_shader_source += base::StringPrintf(
        "texture2D(texture_%s, v_tex_coord_%s).%s", textures[i].name.c_str(),
        textures[i].name.c_str(), textures[i].components.c_str());
    components_used += textures[i].components.length();
  }
  if (components_used == 3) {
    // Add an alpha component of 1.
    blit_fragment_shader_source += ", 1.0";
  }
  if (!tonemapping) {
    if (color_matrix) {
      blit_fragment_shader_source +=
          ");"
          "  vec4 color = untransformed_color * to_rgb_color_matrix;";
    } else {
      blit_fragment_shader_source +=
          ");"
          "  vec4 color = untransformed_color;";
    }
  } else {
    if (color_matrix) {
      blit_fragment_shader_source +=
          ");\n"
          "float Y = untransformed_color.x * 64. - 0.0625;\n"
          "float Cb = untransformed_color.y * 64. - 0.5;\n"
          "float Cr = untransformed_color.z * 64. - 0.5;\n";
    } else {
      blit_fragment_shader_source +=
          ");\n"
          // ITU-R BT.2020-2:
          "float Y = 0.2627 * untransformed_color.x + 0.6780 * \n"
          "untransformed_color.y + 0.0593 * untransformed_color.z;\n"
          "float Cb = 0.531519 * (untransformed_color.z - Y);\n"
          "float Cr = 0.67815 * (untransformed_color.x - Y);\n";
    }
    blit_fragment_shader_source +=
        // Tone mapping step 1.
        "float rho_hdr = 13.2598;\n"  // 1 + 32 * (L_hdr / 10000)^(1/2.4)
                                      // L_hdr = 1000
        "float rho_sdr = 5.69695;\n"  // 1 + 32 * (L_sdr / 10000)^(1/2.4)
                                      // L_sdr = 290
        "float Y_p = log(1. + (rho_hdr - 1.) * Y) / log(rho_hdr);\n"
        // Tone mapping step 2.
        "float Y_c = 1.0770 * Y_p;\n"
        "if (Y_p > 0.7399 && Y_p < 0.9909)\n"
        "  Y_c = -1.1510 * Y_p * Y_p +  2.7811 * Y_p - 0.6302;\n"
        "if (Y_p >= 0.9909)\n"
        "  Y_c = 0.5 * Y_p + 0.5;\n"
        // Tone mapping step 3.
        "float Y_sdr = (pow(rho_sdr, Y_c) - 1.) / (rho_sdr - 1.);\n"
        "float color_scaling = Y_sdr /Y / 1.1;\n"
        "float Cb_tmo = Cb * color_scaling;\n"
        "float Cr_tmo = Cr * color_scaling;\n"
        "float Y_tmo = Y_sdr - max(0.1 * Cr_tmo, 0.);\n";
    if (color_matrix) {
      blit_fragment_shader_source +=
          "vec4 corrected_color = vec4( (Y_tmo + 0.0625) / 64.,"
          "(Cb_tmo + 0.5) / 64.,  (Cr_tmo + 0.5) / 64.,"
          "untransformed_color.w);\n"
          "vec4 color = corrected_color * to_rgb_color_matrix;\n";
    } else {
      blit_fragment_shader_source +=
          "vec4 color;\n"
          "color.x = Y_tmo + 1.4746*Cr_tmo;\n"
          "color.y = Y_tmo - 0.5714*Cr_tmo - 0.1646*Cb_tmo;\n"
          "color.z = Y_tmo + 1.8814*Cb_tmo;\n"
          "color.w = untransformed_color.w;\n";
    }
    blit_fragment_shader_source +=
        "const float kRefWhiteLevelSRGB = 290.0;\n"
        "const float kRefWhiteLevelPQ = 1000.0;\n"
        "const float c1 = 3424.0/4096.0;\n"
        "const float c2 = 2413.0*32.0/4096.0;\n"
        "const float c3 = 2392.0*32.0/4096.0;\n"
        "const float m1 = 2610.0/4096.0/4.0;\n"
        "const float m2 = 2523.0*128.0/4096.0;\n"
        "color.xyz = max(color.xyz, vec3(0.0));\n"
        "vec3 M = c2 - c3 * pow(color.xyz, vec3(1.0 / m2));\n"
        "vec3 N = pow(color.xyz, vec3(1.0 / m2)) - c1;\n"
        "vec3 L = pow(max(N/M, vec3(0.0)), vec3(1.0 /m1));\n"
        "L = L * kRefWhiteLevelPQ;\n"
        "color.x = L.x * (82250100.0/49532999.0) + L.y * "
        "(-29111068.0/49532999.0) + L.z * (-3606033.0/49532999.0);\n"
        "color.y = L.x * (-6169900.0/49532999.0) + L.y * "
        "(56118932.0/49532999.0) + L.z * (-416033.0/49532999.0);\n"
        "color.z = L.x * (-899900.0/49532999.0) + L.y * "
        "(-4981068.0/49532999.0) + L.z * (55413967.0/49532999.0);\n"
        "L = color.xyz / kRefWhiteLevelSRGB;\n"
        "color.xyz = L * 12.92;\n"
        "if (L.x > 0.0031308) color.x = 1.055 * pow(L.x, 1.0 / 2.4) - 0.055;\n"
        "if (L.y > 0.0031308) color.y = 1.055 * pow(L.y, 1.0 / 2.4) - 0.055;\n"
        "if (L.z > 0.0031308) color.z = 1.055 * pow(L.z, 1.0 / 2.4) - 0.055;\n";
  }

  const CobaltExtensionGraphicsApi* graphics_extension =
      static_cast<const CobaltExtensionGraphicsApi*>(
          SbSystemGetExtension(kCobaltExtensionGraphicsName));
  CobaltExtensionGraphicsMapToMeshColorAdjustment color_adjustment;
  if (graphics_extension != nullptr &&
      strcmp(graphics_extension->name, kCobaltExtensionGraphicsName) == 0 &&
      graphics_extension->version >= 5 &&
      graphics_extension->GetMapToMeshColorAdjustments(&color_adjustment)) {
    // Setup vectors for the color adjustments. Ensure they use dot for decimal
    // point regardless of locale.
    std::string buffer;
    buffer = "  vec4 scale0 = vec4(" +
             base::NumberToString(color_adjustment.rgba0_scale[0]) + "," +
             base::NumberToString(color_adjustment.rgba0_scale[1]) + "," +
             base::NumberToString(color_adjustment.rgba0_scale[2]) + "," +
             base::NumberToString(color_adjustment.rgba0_scale[3]) + ");" +
             "  vec4 scale1 = vec4(" +
             base::NumberToString(color_adjustment.rgba1_scale[0]) + "," +
             base::NumberToString(color_adjustment.rgba1_scale[1]) + "," +
             base::NumberToString(color_adjustment.rgba1_scale[2]) + "," +
             base::NumberToString(color_adjustment.rgba1_scale[3]) + ");" +
             "  vec4 scale2 = vec4(" +
             base::NumberToString(color_adjustment.rgba2_scale[0]) + "," +
             base::NumberToString(color_adjustment.rgba2_scale[1]) + "," +
             base::NumberToString(color_adjustment.rgba2_scale[2]) + "," +
             base::NumberToString(color_adjustment.rgba2_scale[3]) + ");" +
             "  vec4 scale3 = vec4(" +
             base::NumberToString(color_adjustment.rgba3_scale[0]) + "," +
             base::NumberToString(color_adjustment.rgba3_scale[1]) + "," +
             base::NumberToString(color_adjustment.rgba3_scale[2]) + "," +
             base::NumberToString(color_adjustment.rgba3_scale[3]) + ");";
    blit_fragment_shader_source +=
        "  vec4 color2 = color * color;"
        "  vec4 color3 = color2 * color;" +
        buffer +
        "  color = scale0 + scale1*color + scale2*color2 + scale3*color3;"
        "  gl_FragColor = clamp(color, vec4(0.0), vec4(1.0));"
        "}";
  } else {
    blit_fragment_shader_source +=
        "  gl_FragColor = color;"
        "}";
  }

  return CompileShader(blit_fragment_shader_source);
}

// static
uint32 TexturedMeshRenderer::CreateUYVYFragmentShader(uint32 texture_target,
                                                      int32 texture_wrap_s) {
  SamplerInfo sampler_info = GetSamplerInfo(texture_target);

  std::string blit_fragment_shader_source = sampler_info.preamble;

  blit_fragment_shader_source += "precision mediump float;";
  blit_fragment_shader_source += "varying vec2 v_tex_coord_uyvy;";
  blit_fragment_shader_source +=
      base::StringPrintf("uniform %s texture_uyvy;", sampler_info.type.c_str());

  // The fragment shader below manually performs horizontal linear interpolation
  // filtering of color values.  Specifically it needs to interpolate the UV
  // values differently than Y values because UV values occur at half the
  // frequency.
  blit_fragment_shader_source +=
      "uniform mat4 to_rgb_color_matrix;"
      "uniform ivec2 texture_size_uyvy;"
      "void main() {"
      // Convert from normalized [0,1] texture coordinates into [0, width]
      // texture space coordinates.
      "  float texture_space_x ="
      "      float(texture_size_uyvy.x) * v_tex_coord_uyvy.x;";
  if (texture_wrap_s == GL_CLAMP_TO_EDGE) {
    // We manually clamp our edges to 0.25 to ensure that the Y components are
    // clamped properly.  We let GL's standard texture wrapping flow take care
    // of the UV clamping (i.e. at [0.5, float(texture_size_uyvy.x) - 0.5]).
    blit_fragment_shader_source +=
        "  texture_space_x = clamp("
        "      texture_space_x, 0.25, float(texture_size_uyvy.x) - 0.25);";
  }
  blit_fragment_shader_source +=
      "  float texel_step_u = 1.0 / float(texture_size_uyvy.x);"
      // As our first (left-most) sample, we choose the pixel that will be on
      // the left side of the UV interpolation.  We add 0.5 to the result in
      // order to get a coordinate representing the center of the pixel.
      "  float sample_1_texture_space = floor(texture_space_x - 0.5) + 0.5;"
      "  float sample_1_normalized ="
      "      sample_1_texture_space * texel_step_u;"
      "  float sample_2_normalized = sample_1_normalized + texel_step_u;"
      "  vec4 sample_1 = texture2D("
      "      texture_uyvy, vec2(sample_1_normalized, v_tex_coord_uyvy.y));"
      "  vec4 sample_2 = texture2D("
      "      texture_uyvy, vec2(sample_2_normalized, v_tex_coord_uyvy.y));"
      // Our lerp progress is how far away |texture_space_x| is from
      // sample 1, or in other words how close it is to sample 2.
      "  float lerp_progress = texture_space_x - sample_1_texture_space;"
      "  vec2 uv_value = mix(sample_1.rb, sample_2.rb, lerp_progress);"
      // Since |lerp_progress| represents progress from the center of the
      // sample 1 pixel (uyvy group) to the center of the sample 2 pixel (uyvy)
      // group, we need to determine whether to interpolate between the two
      // y values within sample 1, the last y value of sample 1 and the first
      // y value of sample 2, or the two y values within sample 2.
      "  float y_value;"
      "  if (lerp_progress < 0.25) {"
      "    y_value = mix(sample_1.g, sample_1.a, lerp_progress * 2.0 + 0.5);"
      "  } else if (lerp_progress < 0.75) {"
      "    y_value = mix(sample_1.a, sample_2.g, lerp_progress * 2.0 - 0.5);"
      "  } else {"
      "    y_value = mix(sample_2.g, sample_2.a, lerp_progress * 2.0 - 1.5);"
      "  }"
      // Perform the YUV->RGB transform and output the color value.
      "  vec4 untransformed_color = vec4(y_value, uv_value.r, uv_value.g, 1.0);"
      "  gl_FragColor = untransformed_color * to_rgb_color_matrix;"
      "}";

  return CompileShader(blit_fragment_shader_source);
}

uint32 TexturedMeshRenderer::CreateYUVCompactedTexturesFragmentShader(
    uint32 texture_target, bool tonemapping) {
  SamplerInfo sampler_info = GetSamplerInfo(texture_target);

  std::string blit_fragment_shader_source = sampler_info.preamble;

  // The fragment shader below performs YUV->RGB transform for
  // compacted 10-bit yuv420 textures
  blit_fragment_shader_source +=
      "precision mediump float;"
      "varying vec2 v_tex_coord_y;"
      "varying vec2 v_tex_coord_u;"
      "varying vec2 v_tex_coord_v;"
      "uniform sampler2D texture_y;"
      "uniform sampler2D texture_u;"
      "uniform sampler2D texture_v;"
      "uniform vec2 viewport_size;"
      "uniform vec2 texture_size_as_unpacked;"
      "uniform vec2 content_region_size;"
      "uniform int enable_filtering;"
      "uniform vec2 texture_size;"
      "uniform mat4 to_rgb_color_matrix;"
      // In a compacted YUV image each channel, Y, U and V, is stored as a
      // separate single-channel image plane. Its pixels are stored in 32-bit
      // and each represents 3 10-bit pixels with 2 bits of gap.
      // In order to extract compacted pixels, the horizontal position at the
      // compacted texture is calculated as:
      // index = compacted_position % 3;
      // decompacted_position = compacted_position / 3;
      // pixel = CompactedTexture.Sample(Sampler, decompacted_position)[index];
      "void main() {"
      // texture size as unpacked, f.i. 2562x1440
      "     float frame_w = texture_size_as_unpacked.x;   \n"
      "     float frame_h = texture_size_as_unpacked.y;   \n"
      // texture size in textels, f.i. 854x1440
      "    float ptex_w = texture_size.x;\n"
      "     float ptex_h = texture_size.y;\n"
      "    float y_step = 1.0 / ptex_h;\n"
      "     float w_mult = 1.0 / ptex_w;\n"
      "     float y = (enable_filtering != 0) ?\n"
      "          floor(v_tex_coord_y.y * frame_h) * y_step :\n"
      "          v_tex_coord_y.y;\n"
      "     float x = (frame_w - 1.0) * v_tex_coord_y.x;\n"
      "     float xi = floor(x);\n"
      "     float xcoord = floor(xi * (1.0 / 3.0));\n"
      "     int xcoord_fr = int(floor(xi - xcoord * 3.0));\n"
      "     xcoord *= w_mult;\n"
      "    float Y_component = texture2D(texture_y, vec2(xcoord, "
      "y))[xcoord_fr];\n"
      "     if (enable_filtering != 0)\n"
      "     {\n"
      "          float cx1 = x - xi;\n"
      "          float cy1 = (v_tex_coord_y.y - y) * frame_h;\n"
      "          float cx0 = 1.0 - cx1;\n"
      "          float cy0 = 1.0 - cy1;\n"
      "          Y_component *= cx0 * cy0;\n"
      "          float y2 = min(y + y_step, y_step * (content_region_size.y - "
      "1.0));\n"
      "       Y_component += cx0 * cy1 * texture2D(texture_y, "
      "vec2(xcoord, y2))[xcoord_fr];\n"
      "          xi = min(xi + 1.0, float(content_region_size.x - 1.0));\n"
      "          xcoord = floor(xi  * (1.0 / 3.0));\n"
      "          xcoord_fr = int(floor(xi - xcoord * 3.0));\n"
      "          xcoord *= w_mult;\n"
      "          Y_component += cx1 * cy0 * texture2D(texture_y, vec2(xcoord, "
      "y))[xcoord_fr];\n"
      "          Y_component += cx1 * cy1 * texture2D(texture_y, vec2(xcoord, "
      "y2))[xcoord_fr];\n"
      "     }\n"
      "      //chroma:\n"
      "      float f_w = frame_w / 2.0;\n"
      "      float f_h = frame_h / 2.0;\n"
      "      y_step = 1.0 / (ptex_h / 2.0);\n"
      "      w_mult = 1.0 / (ptex_w / 2.0);\n"
      "      y = (enable_filtering != 0) ?\n"
      "          floor(v_tex_coord_y.y * f_h) * y_step :\n"
      "          v_tex_coord_y.y;\n"
      "      x = (f_w - 1.0) * v_tex_coord_y.x;\n"
      "      xi = floor(x);\n"
      "      xcoord = floor(xi * (1.0 / 3.0));\n"
      "      xcoord_fr = int(floor(xi - xcoord * 3.0));\n"
      "      xcoord *= w_mult;\n"
      "      float U_component = texture2D(texture_u, vec2(xcoord, "
      "y))[xcoord_fr];\n"
      "      float V_component = texture2D(texture_v, vec2(xcoord, "
      "y))[xcoord_fr];\n"
      "      if (enable_filtering != 0)\n"
      "      {\n"
      "          float cx1 = x - xi;\n"
      "          float cy1 = (v_tex_coord_y.y - y) * f_h;\n"
      "          float coef0 = (1.0 - cx1) * (1.0 - cy1);\n"
      "          float coef2 = cx1 * (1.0 - cy1);\n"
      "          float coef1 = (1.0 - cx1) * cy1;\n"
      "          float coef3 = cx1 * cy1;\n"
      "          U_component *= coef0;\n"
      "          V_component *= coef0;\n"
      "          float y2 = min(y + y_step, y_step * (content_region_size.y / "
      "2.0 - "
      "1.0));\n"
      "          U_component += coef1 * texture2D(texture_u, vec2(xcoord, "
      "y2))[xcoord_fr];\n"
      "          V_component += coef1 * texture2D(texture_v, vec2(xcoord, "
      "y2))[xcoord_fr];\n"
      "          xi = min(xi + 1.0, float(content_region_size.x / 2.0 - "
      "1.0));\n"
      "          xcoord = floor(xi  * (1.0 / 3.0));\n"
      "          xcoord_fr = int(floor(xi - xcoord * 3.0));\n"
      "          xcoord *= w_mult;\n"
      "          U_component += coef2 * texture2D(texture_u, vec2(xcoord, "
      "y))[xcoord_fr];\n"
      "          V_component += coef2 * texture2D(texture_v, vec2(xcoord, "
      "y))[xcoord_fr];\n"
      "          U_component += coef3 * texture2D(texture_u, vec2(xcoord, "
      "y2))[xcoord_fr];\n"
      "          V_component += coef3 * texture2D(texture_v, vec2(xcoord, "
      "y2))[xcoord_fr];\n"
      "      }\n"
      "    vec4 untransformed_color = vec4(Y_component, U_component, "
      "V_component, 1.0);\n";
  if (!tonemapping) {
    blit_fragment_shader_source +=
        "gl_FragColor = untransformed_color * to_rgb_color_matrix;\n"
        "}";
  } else {
    blit_fragment_shader_source +=
        "float Y = untransformed_color.x - 0.0625;\n"
        "float Cb = untransformed_color.y - 0.5;\n"
        "float Cr = untransformed_color.z - 0.5;\n"
        // Tone mapping step 1.
        "float rho_hdr = 13.2598;\n"  // 1 + 32 * (L_hdr / 10000)^(1/2.4)
                                      // L_hdr = 1000
        "float rho_sdr = 5.69695;\n"  // 1 + 32 * (L_sdr / 10000)^(1/2.4)
                                      // L_sdr = 290
        "float Y_p = log(1. + (rho_hdr - 1.) * Y) / log(rho_hdr);\n"
        // Tone mapping step 2.
        "float Y_c = 1.0770 * Y_p;\n"
        "if (Y_p > 0.7399 && Y_p < 0.9909)\n"
        "  Y_c = -1.1510 * Y_p * Y_p +  2.7811 * Y_p - 0.6302;\n"
        "if (Y_p >= 0.9909)\n"
        "  Y_c = 0.5 * Y_p + 0.5;\n"
        // Tone mapping step 3.
        "float Y_sdr = (pow(rho_sdr, Y_c) - 1.) / (rho_sdr - 1.);\n"
        "float color_scaling = Y_sdr /Y / 1.1;\n"
        "float Cb_tmo = Cb * color_scaling;\n"
        "float Cr_tmo = Cr * color_scaling;\n"
        "float Y_tmo = Y_sdr - max(0.1 * Cr_tmo, 0.);\n"
        "vec4 corrected_color = vec4(Y_tmo + 0.0625,"
        "Cb_tmo + 0.5, Cr_tmo + 0.5, untransformed_color.w);\n"
        "vec4 color = corrected_color * to_rgb_color_matrix;\n"
        "const float kRefWhiteLevelSRGB = 290.0;\n"
        "const float kRefWhiteLevelPQ = 1000.0;\n"
        "const float c1 = 3424.0/4096.0;\n"
        "const float c3 = 2392.0*32.0/4096.0;\n"
        "const float c2 = 2413.0*32.0/4096.0;\n"
        "const float m1 = 2610.0/4096.0/4.0;\n"
        "const float m2 = 2523.0*128.0/4096.0;\n"
        "color.xyz = max(color.xyz, vec3(0.0));\n"
        "vec3 M = c2 - c3 * pow(color.xyz, vec3(1.0 / m2));\n"
        "vec3 N = pow(color.xyz, vec3(1.0 / m2)) - c1;\n"
        "vec3 L = pow(max(N/M, vec3(0.0)), vec3(1.0 /m1));\n"
        "L = L * kRefWhiteLevelPQ;\n"
        "color.x = L.x * (82250100.0/49532999.0) + L.y * "
        "(-29111068.0/49532999.0) + L.z * (-3606033.0/49532999.0);\n"
        "color.y = L.x * (-6169900.0/49532999.0) + L.y * "
        "(56118932.0/49532999.0) + L.z * (-416033.0/49532999.0);\n"
        "color.z = L.x * (-899900.0/49532999.0) + L.y * "
        "(-4981068.0/49532999.0) + L.z * (55413967.0/49532999.0);\n"
        "L = color.xyz / kRefWhiteLevelSRGB;\n"
        "color.xyz = L * 12.92;\n"
        "if (L.x > 0.0031308) color.x = 1.055 * pow(L.x, 1.0 / 2.4) - 0.055;\n"
        "if (L.y > 0.0031308) color.y = 1.055 * pow(L.y, 1.0 / 2.4) - 0.055;\n"
        "if (L.z > 0.0031308) color.z = 1.055 * pow(L.z, 1.0 / 2.4) - 0.055;\n"
        "gl_FragColor = color;\n"
        "}";
  }
  return CompileShader(blit_fragment_shader_source);
}

// static
TexturedMeshRenderer::ProgramInfo TexturedMeshRenderer::MakeBlitProgram(
    const float* color_matrix, const std::vector<TextureInfo>& textures,
    uint32 blit_fragment_shader) {
  int total_components = 0;
  for (unsigned int i = 0; i < textures.size(); ++i) {
    total_components += textures[i].components.length();
  }
  DCHECK(total_components == 3 || total_components == 4);

  ProgramInfo result;

  // Create the blit program.
  // Setup shaders used when blitting the current texture.
  result.gl_program_id = GL_CALL_SIMPLE(glCreateProgram());

  uint32 blit_vertex_shader = CreateVertexShader(textures);
  GL_CALL(glAttachShader(result.gl_program_id, blit_vertex_shader));

  GL_CALL(glAttachShader(result.gl_program_id, blit_fragment_shader));

  GL_CALL(glBindAttribLocation(result.gl_program_id, kBlitPositionAttribute,
                               "a_position"));
  GL_CALL(glBindAttribLocation(result.gl_program_id, kBlitTexcoordAttribute,
                               "a_tex_coord"));

  GL_CALL(glLinkProgram(result.gl_program_id));

  result.mvp_transform_uniform = GL_CALL_SIMPLE(glGetUniformLocation(
      result.gl_program_id, "model_view_projection_transform"));
  for (unsigned int i = 0; i < textures.size(); ++i) {
    std::string scale_translate_uniform_name =
        base::StringPrintf("scale_translate_%s", textures[i].name.c_str());
    result.texcoord_scale_translate_uniforms[i] =
        GL_CALL_SIMPLE(glGetUniformLocation(
            result.gl_program_id, scale_translate_uniform_name.c_str()));
    DCHECK_EQ(GL_NO_ERROR, GL_CALL_SIMPLE(glGetError()));

    std::string texture_uniform_name =
        base::StringPrintf("texture_%s", textures[i].name.c_str());
    result.texture_uniforms[i] = GL_CALL_SIMPLE(glGetUniformLocation(
        result.gl_program_id, texture_uniform_name.c_str()));
    DCHECK_EQ(GL_NO_ERROR, GL_CALL_SIMPLE(glGetError()));

    std::string texture_size_name =
        base::StringPrintf("texture_size_%s", textures[i].name.c_str());
    result.texture_size_uniforms[i] = GL_CALL_SIMPLE(
        glGetUniformLocation(result.gl_program_id, texture_size_name.c_str()));
    DCHECK_EQ(GL_NO_ERROR, GL_CALL_SIMPLE(glGetError()));
  }

  // Upload the color matrix right away since it won't change from draw to draw.
  if (color_matrix) {
    GL_CALL(glUseProgram(result.gl_program_id));
    uint32 to_rgb_color_matrix_uniform = GL_CALL_SIMPLE(
        glGetUniformLocation(result.gl_program_id, "to_rgb_color_matrix"));
    GL_CALL(glUniformMatrix4fv(to_rgb_color_matrix_uniform, 1, GL_FALSE,
                               color_matrix));
    if (color_matrix == k10BitBT2020ColorMatrixCompacted) {
      result.viewport_size_uniform = GL_CALL_SIMPLE(
          glGetUniformLocation(result.gl_program_id, "viewport_size"));
      DCHECK_EQ(GL_NO_ERROR, GL_CALL_SIMPLE(glGetError()));
      result.subtexture_size_uniform = GL_CALL_SIMPLE(
          glGetUniformLocation(result.gl_program_id, "texture_size"));
      DCHECK_EQ(GL_NO_ERROR, GL_CALL_SIMPLE(glGetError()));
      result.texture_size_as_unpacked_uniform =
          GL_CALL_SIMPLE(glGetUniformLocation(result.gl_program_id,
                                              "texture_size_as_unpacked"));
      DCHECK_EQ(GL_NO_ERROR, GL_CALL_SIMPLE(glGetError()));
      result.enable_filtering_uniform = GL_CALL_SIMPLE(
          glGetUniformLocation(result.gl_program_id, "enable_filtering"));
      DCHECK_EQ(GL_NO_ERROR, GL_CALL_SIMPLE(glGetError()));
      result.content_region_size_uniform = GL_CALL_SIMPLE(
          glGetUniformLocation(result.gl_program_id, "content_region_size"));
      DCHECK_EQ(GL_NO_ERROR, GL_CALL_SIMPLE(glGetError()));
    }
  }

  GL_CALL(glUseProgram(0));

  GL_CALL(glDeleteShader(blit_fragment_shader));
  GL_CALL(glDeleteShader(blit_vertex_shader));

  return result;
}

TexturedMeshRenderer::ProgramInfo TexturedMeshRenderer::GetBlitProgram(
    const Image& image) {
  Image::Type type = image.type;
  uint32 texture_target = image.textures[0].texture->GetTarget();
  base::Optional<int32> texture_wrap_s;
  if (type == Image::YUV_UYVY_422_BT709) {
    texture_wrap_s.emplace();
    GL_CALL(
        glBindTexture(texture_target, image.textures[0].texture->gl_handle()));
    GL_CALL(glGetTexParameteriv(texture_target, GL_TEXTURE_WRAP_S,
                                &(*texture_wrap_s)));
    GL_CALL(glBindTexture(texture_target, 0));
  }

  bool tonemapping = !IsHdrAngleModeEnabled() &&
                     image.transfer_id == kSbMediaTransferIdSmpteSt2084;

  CacheKey key(texture_target, type, tonemapping, texture_wrap_s);

  ProgramCache::iterator found = blit_program_cache_.find(key);
  if (found == blit_program_cache_.end()) {
    const float* color_matrix = GetColorMatrixForImageType(type);
    ProgramInfo result;
    switch (type) {
      case Image::RGBA: {
        std::vector<TextureInfo> texture_infos;
        texture_infos.push_back(TextureInfo("rgba", "rgba"));
        result =
            MakeBlitProgram(color_matrix, texture_infos,
                            CreateFragmentShader(texture_target, texture_infos,
                                                 color_matrix, tonemapping));
      } break;
      case Image::YUV_2PLANE_BT709: {
        std::vector<TextureInfo> texture_infos;
        switch (image.textures[0].texture->GetFormat()) {
          case GL_ALPHA:
            texture_infos.push_back(TextureInfo("y", "a"));
            break;
#if defined(GL_RED_EXT)
          case GL_RED_EXT:
            texture_infos.push_back(TextureInfo("y", "r"));
            break;
#endif
          default:
            NOTREACHED();
        }
        switch (image.textures[1].texture->GetFormat()) {
          case GL_LUMINANCE_ALPHA:
            texture_infos.push_back(TextureInfo("uv", "ba"));
            break;
#if defined(GL_RG_EXT)
          case GL_RG_EXT:
            texture_infos.push_back(TextureInfo("uv", "rg"));
            break;
#endif
          default:
            NOTREACHED();
        }
        result = MakeBlitProgram(
            color_matrix, texture_infos,
            CreateFragmentShader(texture_target, texture_infos, color_matrix));
      } break;
      case Image::YUV_3PLANE_BT601_FULL_RANGE:
      case Image::YUV_3PLANE_BT709:
      case Image::YUV_3PLANE_10BIT_BT2020:
      case Image::YUV_3PLANE_10BIT_COMPACT_BT2020: {
        std::vector<TextureInfo> texture_infos;
#if defined(GL_RED_EXT)
        if (image.textures[0].texture->GetFormat() == GL_RED_EXT) {
          texture_infos.push_back(TextureInfo("y", "r"));
        } else {
          texture_infos.push_back(TextureInfo("y", "a"));
        }
        if (image.textures[1].texture->GetFormat() == GL_RED_EXT) {
          texture_infos.push_back(TextureInfo("u", "r"));
        } else {
          texture_infos.push_back(TextureInfo("u", "a"));
        }
        if (image.textures[2].texture->GetFormat() == GL_RED_EXT) {
          texture_infos.push_back(TextureInfo("v", "r"));
        } else {
          texture_infos.push_back(TextureInfo("v", "a"));
        }
#else   // defined(GL_RED_EXT)
        texture_infos.push_back(TextureInfo("y", "a"));
        texture_infos.push_back(TextureInfo("u", "a"));
        texture_infos.push_back(TextureInfo("v", "a"));
#endif  // defined(GL_RED_EXT)
        uint32 shader_program;
        if (type == Image::YUV_3PLANE_10BIT_COMPACT_BT2020) {
          shader_program = CreateYUVCompactedTexturesFragmentShader(
              texture_target, tonemapping);
        } else {
          shader_program = CreateFragmentShader(texture_target, texture_infos,
                                                color_matrix, tonemapping);
        }
        result = MakeBlitProgram(color_matrix, texture_infos, shader_program);
      } break;
      case Image::YUV_UYVY_422_BT709: {
        std::vector<TextureInfo> texture_infos;
        texture_infos.push_back(TextureInfo("uyvy", "rgba"));
        result = MakeBlitProgram(
            color_matrix, texture_infos,
            CreateUYVYFragmentShader(texture_target, *texture_wrap_s));
      } break;
      default: {
        NOTREACHED();
      }
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
