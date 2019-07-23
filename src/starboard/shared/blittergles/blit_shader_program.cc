// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/blittergles/blit_shader_program.h"

#include <GLES2/gl2.h>

#include "starboard/blitter.h"
#include "starboard/shared/blittergles/blitter_context.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/blittergles/blitter_surface.h"
#include "starboard/shared/blittergles/shader_program.h"
#include "starboard/shared/gles/gl_call.h"

namespace starboard {
namespace shared {
namespace blittergles {

namespace {

// Location of the blit shader attribute "a_blit_position."
static const int kBlitPositionAttribute = 0;

// Location of the blit shader attribute "a_tex_coord."
static const int kTexCoordAttribute = 1;

// When applied to a color vector, this 4x4 identity matrix will not affect it:
// kIdentityMatrix * [r g b a]^T = [r g b a]^T.
const float kIdentityMatrix[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                   0, 0, 1, 0, 0, 0, 0, 1};

// When applied to a color vector, this 4x4 matrix will fill rgb values with the
// alpha value: kAlphaMatrix * [r g b a]^T = [a a a a]^T. Note GL matrices are
// read in column-major order.
const float kAlphaMatrix[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1};

}  // namespace

BlitShaderProgram::BlitShaderProgram() {
  const char* vertex_shader_source =
      "attribute vec2 a_blit_position;"
      "attribute vec2 a_tex_coord;"
      "varying vec2 v_tex_coord;"
      "void main() {"
      "  gl_Position = vec4(a_blit_position.x, a_blit_position.y, 0, 1);"
      "  v_tex_coord = a_tex_coord;"
      "}";
  const char* fragment_shader_source =
      "precision mediump float;"
      "uniform sampler2D tex;"
      "uniform mat4 u_color_matrix;"
      "uniform vec4 u_tex_coord_clamp;"
      "varying vec2 v_tex_coord;"
      "void main() {"
      "  vec2 coords = clamp(v_tex_coord, u_tex_coord_clamp.xy, "
      "                      u_tex_coord_clamp.zw);"
      "  gl_FragColor = u_color_matrix * texture2D(tex, coords);"
      "}";
  InitializeShaders(vertex_shader_source, fragment_shader_source);
  GL_CALL(glBindAttribLocation(GetProgramHandle(), kBlitPositionAttribute,
                               "a_blit_position"));
  GL_CALL(glBindAttribLocation(GetProgramHandle(), kTexCoordAttribute,
                               "a_tex_coord"));

  int link_status;
  GL_CALL(glLinkProgram(GetProgramHandle()));
  GL_CALL(glGetProgramiv(GetProgramHandle(), GL_LINK_STATUS, &link_status));
  SB_CHECK(link_status);

  clamp_uniform_ =
      glGetUniformLocation(GetProgramHandle(), "u_tex_coord_clamp");
  color_matrix_uniform_ =
      glGetUniformLocation(GetProgramHandle(), "u_color_matrix");
  SB_CHECK(clamp_uniform_ != -1);
  SB_CHECK(color_matrix_uniform_ != -1);
}

bool BlitShaderProgram::Draw(SbBlitterContext context,
                             SbBlitterSurface surface,
                             SbBlitterRect src_rect,
                             SbBlitterRect dst_rect) const {
  GL_CALL(glUseProgram(GetProgramHandle()));

  float dst_vertices[8], src_vertices[8];
  SetTexCoords(src_rect, surface->info.width, surface->info.height,
               src_vertices);
  SetNDC(dst_rect, context->current_render_target->width,
         context->current_render_target->height, dst_vertices);

  // Clamp so fragment shader does not sample beyond edges of texture.
  const float kTexelInset = 0.499f;
  float texel_clamps[] = {
      src_vertices[0] + kTexelInset / surface->info.width,    // min u
      src_vertices[1] + kTexelInset / surface->info.height,   // min v
      src_vertices[4] - kTexelInset / surface->info.width,    // max u
      src_vertices[3] - kTexelInset / surface->info.height};  // max v
  GL_CALL(glVertexAttribPointer(kBlitPositionAttribute, 2, GL_FLOAT, GL_FALSE,
                                0, dst_vertices));
  GL_CALL(glVertexAttribPointer(kTexCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0,
                                src_vertices));
  GL_CALL(glEnableVertexAttribArray(kBlitPositionAttribute));
  GL_CALL(glEnableVertexAttribArray(kTexCoordAttribute));
  GL_CALL(glUniform4f(clamp_uniform_, texel_clamps[0], texel_clamps[1],
                      texel_clamps[2], texel_clamps[3]));

  const float* transform_mat;
  const float kAlphaBlitMatrix[16] =
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, context->current_rgba[0],
       context->current_rgba[1], context->current_rgba[2],
       context->current_rgba[3]};
  const float kBlitMatrix[16] = {context->current_rgba[0], 0, 0, 0, 0,
                                 context->current_rgba[1], 0, 0, 0, 0,
                                 context->current_rgba[2], 0, 0, 0, 0,
                                 context->current_rgba[3]};
  if (surface->info.format == kSbBlitterSurfaceFormatA8) {
    transform_mat =
        context->modulate_blits_with_color ? kAlphaBlitMatrix : kAlphaMatrix;
  } else {
    transform_mat =
        context->modulate_blits_with_color ? kBlitMatrix : kIdentityMatrix;
  }
  GL_CALL(
      glUniformMatrix4fv(color_matrix_uniform_, 1, GL_FALSE, transform_mat));
  GL_CALL(glActiveTexture(GL_TEXTURE0));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, surface->color_texture_handle));

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  bool success = glGetError() == GL_NO_ERROR;

  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  GL_CALL(glDisableVertexAttribArray(kTexCoordAttribute));
  GL_CALL(glDisableVertexAttribArray(kBlitPositionAttribute));
  GL_CALL(glUseProgram(0));
  return success;
}

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard
