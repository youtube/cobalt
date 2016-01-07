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

#include "glimp/gles/program.h"

namespace glimp {
namespace gles {

Program::Program(nb::scoped_ptr<ProgramImpl> impl)
    : impl_(impl.Pass()), link_status_(GL_FALSE) {}

bool Program::AttachShader(const nb::scoped_refptr<Shader>& shader) {
  if (shader->type() == GL_VERTEX_SHADER) {
    if (vertex_shader_) {
      return false;
    }
    vertex_shader_ = shader;
  } else if (shader->type() == GL_FRAGMENT_SHADER) {
    if (fragment_shader_) {
      return false;
    }
    fragment_shader_ = shader;
  } else {
    SB_DLOG(FATAL) << "Invalid shader type.";
  }

  return true;
}

void Program::Link() {
  if (!vertex_shader_ || !fragment_shader_) {
    // We cannot successfully link if both a vertex and fragment shader
    // have not yet been attached.
    link_status_ = GL_FALSE;
    return;
  }

  if (impl_->Link(vertex_shader_, fragment_shader_)) {
    link_status_ = GL_TRUE;
  } else {
    link_status_ = GL_FALSE;
  }
}

void Program::BindAttribLocation(GLuint index, const GLchar* name) {
  // TODO(***REMOVED***): Will come back to this later when the implementation can be
  //               be more easily verified.
  SB_NOTIMPLEMENTED();
}

}  // namespace gles
}  // namespace glimp
