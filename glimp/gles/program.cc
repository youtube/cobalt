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
#include "starboard/common/string.h"
#include "starboard/memory.h"

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

    ClearUniforms();

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
  *length = starboard::strlcpy(infolog, link_results_.info_log.c_str(),
                               bufsize);
}

GLint Program::GetUniformLocation(const GLchar* name) {
  SB_DCHECK(linked());
  int location = impl_->GetUniformLocation(name);
  if (location != -1) {
    if (std::find(active_uniform_locations_.begin(),
                  active_uniform_locations_.end(),
                  location) == active_uniform_locations_.end()) {
      active_uniform_locations_.push_back(location);
    }
  }
  return location;
}

GLenum Program::Uniformiv(GLint location,
                          GLsizei count,
                          GLsizei elem_size,
                          const GLint* v) {
  return UpdateUniform(location, count, elem_size, v,
                       UniformInfo::kTypeInteger);
}

GLenum Program::Uniformfv(GLint location,
                          GLsizei count,
                          GLsizei elem_size,
                          const GLfloat* v) {
  return UpdateUniform(location, count, elem_size, v, UniformInfo::kTypeFloat);
}

GLenum Program::UniformMatrixfv(GLint location,
                                GLsizei count,
                                GLsizei dim_size,
                                const GLfloat* value) {
  return UpdateUniform(location, count, dim_size, value,
                       UniformInfo::kTypeMatrix);
}

Program::Uniform* Program::FindOrMakeUniform(int location) {
  if (std::find(active_uniform_locations_.begin(),
                active_uniform_locations_.end(),
                location) == active_uniform_locations_.end()) {
    return NULL;
  }

  for (size_t i = 0; i < uniforms_.size(); ++i) {
    if (uniforms_[i].location == location) {
      return &uniforms_[i];
    }
  }
  uniforms_.push_back(Uniform());
  uniforms_.back().location = location;
  return &uniforms_.back();
}

// Clear all stored uniform information and values.
void Program::ClearUniforms() {
  for (size_t i = 0; i < uniforms_.size(); ++i) {
    SbMemoryDeallocate(uniforms_[i].data);
  }
  uniforms_.clear();
  active_uniform_locations_.clear();
}

namespace {
int DataSizeForType(GLsizei count, GLsizei elem_size, UniformInfo::Type type) {
  switch (type) {
    case UniformInfo::kTypeInteger:
      return sizeof(int) * count * elem_size;
    case UniformInfo::kTypeFloat:
      return sizeof(float) * count * elem_size;
    case UniformInfo::kTypeMatrix:
      return sizeof(float) * count * elem_size * elem_size;
    default:
      SB_NOTREACHED();
      return NULL;
  }
}
}  // namespace

// Assign the specified data to the specified uniform, so that it is available
// to the next draw call.
GLenum Program::UpdateUniform(GLint location,
                              GLsizei count,
                              GLsizei elem_size,
                              const void* v,
                              UniformInfo::Type type) {
  // TODO: It would be nice to be able to query the ProgramImpl object for
  //       UniformInfo information so that we can check it against incoming
  //       glUniform() calls to ensure consistency.  As it is currently, we are
  //       defining this information through these glUniform() calls.
  Uniform* uniform = FindOrMakeUniform(location);
  if (uniform == NULL) {
    return GL_INVALID_OPERATION;
  }

  UniformInfo new_info = UniformInfo(type, count, elem_size);
  if (new_info != uniform->info) {
    // We need to reallocate data if the information has changed.
    uniform->info = new_info;

    SbMemoryDeallocate(uniform->data);
    uniform->data = SbMemoryAllocate(DataSizeForType(count, elem_size, type));
  }
  memcpy(uniform->data, v, DataSizeForType(count, elem_size, type));

  return GL_NO_ERROR;
}

}  // namespace gles
}  // namespace glimp
