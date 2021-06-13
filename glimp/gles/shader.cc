/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "glimp/gles/shader.h"
#include "starboard/common/string.h"

namespace glimp {
namespace gles {

Shader::Shader(nb::scoped_ptr<ShaderImpl> impl, GLenum type)
    : impl_(impl.Pass()), type_(type), compile_results_(false) {}

void Shader::ShaderSource(GLsizei count,
                          const GLchar* const* string,
                          const GLint* length) {
  source_.clear();

  // Concatenate the input strings into one long string.
  for (size_t i = 0; i < count; ++i) {
    if (!length) {
      // If length is NULL, all strings are assumed to be null-terminated.
      source_.append(string[i]);
    } else {
      source_.append(string[i], length[i]);
    }
  }
}

void Shader::CompileShader() {
  compile_results_ = impl_->Compile(source_);
}

GLenum Shader::GetShaderiv(GLenum pname, GLint* params) {
  switch (pname) {
    case GL_COMPILE_STATUS:
      *params = compile_results_.success ? 1 : 0;
      break;
    case GL_SHADER_SOURCE_LENGTH:
      *params = source_.size();
      break;
    case GL_SHADER_TYPE:
      *params = type_;
      break;
    case GL_INFO_LOG_LENGTH:
      *params = compile_results_.info_log.size();
      break;
    case GL_DELETE_STATUS:
      SB_NOTIMPLEMENTED();
      break;
    default:
      return GL_INVALID_ENUM;
  }

  return GL_NO_ERROR;
}

void Shader::GetShaderInfoLog(GLsizei bufsize,
                              GLsizei* length,
                              GLchar* infolog) {
  *length = starboard::strlcpy(infolog, compile_results_.info_log.c_str(),
                               bufsize);
}

}  // namespace gles
}  // namespace glimp
