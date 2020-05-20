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

#include "cobalt/renderer/rasterizer/egl/shader_base.h"

#include "base/logging.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

void ShaderBase::BindAttribLocation(GLuint program, GLuint location,
                                    const char* name) {
  GL_CALL(glBindAttribLocation(program, location, name));
}

GLuint ShaderBase::GetUniformLocation(GLuint program, const char* name) {
  GLint location = GL_CALL_SIMPLE(glGetUniformLocation(program, name));
  DCHECK_NE(location, -1);
  return static_cast<GLuint>(location);
}

void ShaderBase::SetTextureUnitForUniformSampler(GLuint uniform_location,
                                                 GLenum texture_unit) {
  GL_CALL(glUniform1i(uniform_location, texture_unit - GL_TEXTURE0));
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
