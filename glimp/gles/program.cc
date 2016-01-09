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

    linked_vertex_shader_ = vertex_shader_;
    linked_fragment_shader_ = fragment_shader_;

    // Re-issue any binding attributes that are defined for this program.
    for (BoundAttributes::const_iterator iter = bound_attrib_locations_.begin();
         iter != bound_attrib_locations_.end(); ++iter) {
      impl_->BindAttribLocation(iter->first, iter->second.c_str());
    }
  } else {
    link_status_ = GL_FALSE;
  }
}

void Program::BindAttribLocation(GLuint index, const GLchar* name) {
  bound_attrib_locations_[index] = std::string(name);

  if (link_status() == GL_TRUE) {
    // If we are linked, then immediately pass this new binding information
    // on to the platform-specific implementation.  Otherwise, this information
    // will all be communicated upon linking.
    impl_->BindAttribLocation(index, name);
  }
}

}  // namespace gles
}  // namespace glimp
