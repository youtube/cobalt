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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/rasterizer/egl/shader_program.h"

#include "base/logging.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {

void CompileShader(GLuint handle, const GLchar* source) {
  GLint source_length = strlen(source);
  GL_CALL(glShaderSource(handle, 1, &source, &source_length));
  GL_CALL(glCompileShader(handle));

  GLint compiled = GL_FALSE;
  GL_CALL(glGetShaderiv(handle, GL_COMPILE_STATUS, &compiled));
  if (compiled != GL_TRUE) {
    GLchar log[2048] = {0};
    GL_CALL_SIMPLE(glGetShaderInfoLog(handle, arraysize(log) - 1, NULL, log));
    DLOG(ERROR) << "shader compile error:\n" << log;
    DLOG(ERROR) << "shader source:\n" << source;
  }
  DCHECK(compiled == GL_TRUE);
}

}  // namespace

ShaderProgramBase::ShaderProgramBase() : handle_(0) {}

ShaderProgramBase::~ShaderProgramBase() { DCHECK_EQ(handle_, 0); }

void ShaderProgramBase::Create(ShaderBase* vertex_shader,
                               ShaderBase* fragment_shader) {
  handle_ = GL_CALL_SIMPLE(glCreateProgram());

  GLuint vertex_shader_handle =
      GL_CALL_SIMPLE(glCreateShader(GL_VERTEX_SHADER));
  CompileShader(vertex_shader_handle, vertex_shader->GetSource());
  GL_CALL(glAttachShader(handle_, vertex_shader_handle));

  GLuint fragment_shader_handle =
      GL_CALL_SIMPLE(glCreateShader(GL_FRAGMENT_SHADER));
  CompileShader(fragment_shader_handle, fragment_shader->GetSource());
  GL_CALL(glAttachShader(handle_, fragment_shader_handle));

  vertex_shader->InitializePreLink(handle_);
  fragment_shader->InitializePreLink(handle_);
  GL_CALL(glLinkProgram(handle_));

  GLint linked = GL_FALSE;
  GL_CALL(glGetProgramiv(handle_, GL_LINK_STATUS, &linked));
  if (linked != GL_TRUE) {
    GLchar log[2048] = {0};
    GL_CALL_SIMPLE(glGetProgramInfoLog(handle_, arraysize(log) - 1, NULL, log));
    DLOG(ERROR) << "shader link error:\n" << log;
    DLOG(ERROR) << "vertex source:\n" << vertex_shader->GetSource();
    DLOG(ERROR) << "fragment source:\n" << fragment_shader->GetSource();
  }
  DCHECK(linked == GL_TRUE);

  vertex_shader->InitializePostLink(handle_);
  fragment_shader->InitializePostLink(handle_);
  GL_CALL(glUseProgram(handle_));
  vertex_shader->InitializePostUse();
  fragment_shader->InitializePostUse();

  GL_CALL(glDeleteShader(vertex_shader_handle));
  GL_CALL(glDeleteShader(fragment_shader_handle));
}

void ShaderProgramBase::Destroy(ShaderBase* vertex_shader,
                                ShaderBase* fragment_shader) {
  DCHECK_NE(handle_, 0);
  GL_CALL(glDeleteProgram(handle_));
  handle_ = 0;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
