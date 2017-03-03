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

#include "cobalt/renderer/rasterizer/egl/shader_program.h"

#include <GLES2/gl2.h>

#include "base/logging.h"
#include "cobalt/renderer/backend/egl/utils.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

ShaderProgramBase::ShaderProgramBase()
    : handle_(0) {}

ShaderProgramBase::~ShaderProgramBase() {
  DCHECK_EQ(handle_, 0);
}

void ShaderProgramBase::Create(ShaderBase* vertex_shader,
                               ShaderBase* fragment_shader) {
  handle_ = glCreateProgram();

  vertex_shader->handle_ = glCreateShader(GL_VERTEX_SHADER);
  const GLchar* vertex_source = vertex_shader->GetSource();
  GLint vertex_source_length = strlen(vertex_source);
  GL_CALL(glShaderSource(vertex_shader->handle_, 1, &vertex_source,
                         &vertex_source_length));
  GL_CALL(glCompileShader(vertex_shader->handle_));
  GL_CALL(glAttachShader(handle_, vertex_shader->handle_));

  fragment_shader->handle_ = glCreateShader(GL_FRAGMENT_SHADER);
  const GLchar* fragment_source = fragment_shader->GetSource();
  GLint fragment_source_length = strlen(fragment_source);
  GL_CALL(glShaderSource(fragment_shader->handle_, 1, &fragment_source,
                         &fragment_source_length));
  GL_CALL(glCompileShader(fragment_shader->handle_));
  GL_CALL(glAttachShader(handle_, fragment_shader->handle_));

  vertex_shader->InitializePreLink(handle_);
  fragment_shader->InitializePreLink(handle_);
  GL_CALL(glLinkProgram(handle_));
  vertex_shader->InitializePostLink(handle_);
  fragment_shader->InitializePostLink(handle_);
  GL_CALL(glUseProgram(handle_));
  vertex_shader->InitializePostUse();
  fragment_shader->InitializePostUse();
}

void ShaderProgramBase::Destroy(ShaderBase* vertex_shader,
                                ShaderBase* fragment_shader) {
  DCHECK_NE(handle_, 0);
  GL_CALL(glDeleteProgram(handle_));
  GL_CALL(glDeleteShader(vertex_shader->handle_));
  GL_CALL(glDeleteShader(fragment_shader->handle_));
  handle_ = 0;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
