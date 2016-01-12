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
#include "starboard/string.h"

namespace glimp {
namespace gles {

Program::Program(nb::scoped_ptr<ProgramImpl> impl)
    : impl_(impl.Pass()), link_results_(false) {}

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
    link_results_ = ProgramImpl::LinkResults(
        false, "A fragment or vertex shader is not attached.");
    return;
  }

  link_results_ = impl_->Link(vertex_shader_, fragment_shader_);
  if (link_results_.success) {
    linked_vertex_shader_ = vertex_shader_;
    linked_fragment_shader_ = fragment_shader_;

    // Re-issue any binding attributes that are defined for this program.
    for (BoundAttributes::const_iterator iter = bound_attrib_locations_.begin();
         iter != bound_attrib_locations_.end(); ++iter) {
      impl_->BindAttribLocation(iter->first, iter->second.c_str());
    }
  }
}

void Program::BindAttribLocation(GLuint index, const GLchar* name) {
  bound_attrib_locations_[index] = std::string(name);

  if (linked()) {
    // If we are linked, then immediately pass this new binding information
    // on to the platform-specific implementation.  Otherwise, this information
    // will all be communicated upon linking.
    impl_->BindAttribLocation(index, name);
  }
}

GLenum Program::GetProgramiv(GLenum pname, GLint* params) {
  switch (pname) {
    case GL_LINK_STATUS:
      *params = (link_results_.success ? 1 : 0);
      break;
    case GL_INFO_LOG_LENGTH:
      *params = link_results_.info_log.size();
      break;
    case GL_DELETE_STATUS:
    case GL_VALIDATE_STATUS:
    case GL_ATTACHED_SHADERS:
    case GL_ACTIVE_ATTRIBUTES:
    case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
    case GL_ACTIVE_UNIFORMS:
    case GL_ACTIVE_UNIFORM_MAX_LENGTH:
      SB_NOTIMPLEMENTED();
      break;
    default:
      return GL_INVALID_ENUM;
  }

  return GL_NO_ERROR;
}

void Program::GetProgramInfoLog(GLsizei bufsize,
                                GLsizei* length,
                                GLchar* infolog) {
  *length = SbStringCopy(infolog, link_results_.info_log.c_str(), bufsize);
}

}  // namespace gles
}  // namespace glimp
