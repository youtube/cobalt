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

namespace glimp {
namespace gles {

Shader::Shader(nb::scoped_ptr<ShaderImpl> impl, GLenum type)
    : impl_(impl.Pass()), type_(type) {}

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

}  // namespace gles
}  // namespace glimp
