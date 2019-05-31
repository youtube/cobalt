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

#include "starboard/shared/blittergles/color_shader_program.h"

#include <GLES2/gl2.h>

#include <utility>

#include "starboard/blitter.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/blittergles/shader_program.h"
#include "starboard/shared/gles/gl_call.h"

namespace starboard {
namespace shared {
namespace blittergles {

namespace {

// Transform the given blitter-space coordinates into NDC.
std::pair<float, float> TransformCoords(
    const int x,
    const int y,
    SbBlitterRenderTargetPrivate* render_target) {
  const float transformed_x = -1 + (2.0f * x) / render_target->width;
  const float transformed_y = 1 - (2.0f * y) / render_target->height;
  return std::pair<float, float>(transformed_x, transformed_y);
}

}  // namespace

ColorShaderProgram::ColorShaderProgram() {
  const char* vertex_shader_source =
      "attribute vec2 a_position;"
      "attribute vec4 a_color;"
      "varying vec4 v_color;"
      "void main() {"
      "  gl_Position = vec4(a_position.x, a_position.y, 0, 1);"
      "  v_color = a_color;"
      "}";
  const char* fragment_shader_source =
      "precision mediump float;"
      "varying vec4 v_color;"
      "void main() {"
      "  gl_FragColor = v_color;"
      "}";
  InitializeShaders(vertex_shader_source, fragment_shader_source);

  // Link program.
  GLuint program_handle = GetProgramHandle();
  GL_CALL(
      glBindAttribLocation(program_handle, kPositionAttribute, "a_position"));
  GL_CALL(glBindAttribLocation(program_handle, kColorAttribute, "a_color"));

  int link_status;
  GL_CALL(glLinkProgram(program_handle));
  GL_CALL(glGetProgramiv(program_handle, GL_LINK_STATUS, &link_status));
  SB_CHECK(link_status);
}

bool ColorShaderProgram::Draw(SbBlitterRenderTarget render_target,
                              SbBlitterColor color,
                              SbBlitterRect rect) const {
  GL_CALL(glUseProgram(GetProgramHandle()));
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
  std::pair<float, float> lower_left_coord =
      TransformCoords(rect.x, rect.y + rect.height, render_target);
  std::pair<float, float> upper_right_coord =
      TransformCoords(rect.x + rect.width, rect.y, render_target);
  float vertices[] = {lower_left_coord.first,  lower_left_coord.second,
                      lower_left_coord.first,  upper_right_coord.second,
                      upper_right_coord.first, lower_left_coord.second,
                      upper_right_coord.first, upper_right_coord.second};
  GL_CALL(glVertexAttribPointer(kPositionAttribute, 2, GL_FLOAT, GL_FALSE, 0,
                                vertices));
  GL_CALL(glEnableVertexAttribArray(kPositionAttribute));

  const float kColorMapper = 255.0;
  GL_CALL(glVertexAttrib4f(kColorAttribute,
                           SbBlitterRFromColor(color) / kColorMapper,
                           SbBlitterGFromColor(color) / kColorMapper,
                           SbBlitterBFromColor(color) / kColorMapper,
                           SbBlitterAFromColor(color) / kColorMapper));

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  bool success = glGetError() == GL_NO_ERROR;

  GL_CALL(glDisableVertexAttribArray(kPositionAttribute));
  GL_CALL(glUseProgram(0));
  return success;
}

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard
