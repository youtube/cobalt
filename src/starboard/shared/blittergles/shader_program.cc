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

#include "starboard/shared/blittergles/shader_program.h"

#include <GLES2/gl2.h>

#include <functional>
#include <utility>

#include "starboard/blitter.h"
#include "starboard/memory.h"
#include "starboard/shared/gles/gl_call.h"
#include "starboard/string.h"

namespace starboard {
namespace shared {
namespace blittergles {

namespace {

const void TransformCoords(
    SbBlitterRect rect,
    float (&vertex_data)[8],
    int width,
    int height,
    std::function<std::pair<float, float>(int, int, int, int)> transform_func) {
  std::pair<float, float> lower_left_coords =
      transform_func(rect.x, rect.y + rect.height, width, height);
  std::pair<float, float> upper_right_coords =
      transform_func(rect.x + rect.width, rect.y, width, height);
  float temp[] = {lower_left_coords.first,  lower_left_coords.second,
                  lower_left_coords.first,  upper_right_coords.second,
                  upper_right_coords.first, lower_left_coords.second,
                  upper_right_coords.first, upper_right_coords.second};
  memcpy(vertex_data, temp, sizeof(float) * 8);
}

}  // namespace

ShaderProgram::~ShaderProgram() {
  GL_CALL(glDeleteProgram(program_handle_));
  GL_CALL(glDeleteShader(vertex_shader_));
  GL_CALL(glDeleteShader(fragment_shader_));
}

void ShaderProgram::InitializeShader(GLuint shader_handle,
                                     const char* shader_source) {
  int shader_source_length = strlen(shader_source);
  int compile_status;
  GL_CALL(
      glShaderSource(shader_handle, 1, &shader_source, &shader_source_length));
  GL_CALL(glCompileShader(shader_handle));
  GL_CALL(glGetShaderiv(shader_handle, GL_COMPILE_STATUS, &compile_status));
  SB_CHECK(compile_status);
  GL_CALL(glAttachShader(program_handle_, shader_handle));
}

void ShaderProgram::InitializeShaders(const char* vertex_shader_source,
                                      const char* fragment_shader_source) {
  program_handle_ = glCreateProgram();
  SB_CHECK(program_handle_ != 0);

  vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
  SB_CHECK(vertex_shader_ != 0);
  InitializeShader(vertex_shader_, vertex_shader_source);

  fragment_shader_ = glCreateShader(GL_FRAGMENT_SHADER);
  SB_CHECK(fragment_shader_ != 0);
  InitializeShader(fragment_shader_, fragment_shader_source);
}

const void ShaderProgram::SetNDC(SbBlitterRect rect,
                                 int width,
                                 int height,
                                 float (&vertex_data)[8]) {
  TransformCoords(rect, vertex_data, width, height,
                  [](int x, int y, int width, int height) {
                    return std::pair<float, float>(-1 + (2.0f * x) / width,
                                                   1 - (2.0f * y) / height);
                  });
}

const void ShaderProgram::SetTexCoords(SbBlitterRect rect,
                                       int width,
                                       int height,
                                       float (&vertex_data)[8]) {
  TransformCoords(rect, vertex_data, width, height,
                  [](int x, int y, int width, int height) {
                    return std::pair<float, float>(
                        static_cast<float>(x) / width,
                        1.0f - (static_cast<float>(y) / height));
                  });
}

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard
