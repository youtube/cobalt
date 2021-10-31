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

#ifndef GLIMP_GLES_PROGRAM_H_
#define GLIMP_GLES_PROGRAM_H_

#include <GLES3/gl3.h>

#include <map>
#include <string>
#include <vector>

#include "glimp/gles/program_impl.h"
#include "glimp/gles/shader.h"
#include "glimp/gles/uniform_info.h"
#include "nb/ref_counted.h"
#include "nb/scoped_ptr.h"

namespace glimp {
namespace gles {

class Program : public nb::RefCountedThreadSafe<Program> {
 public:
  // Represents a Uniform entry in this program, including type information
  // about it and the actual data as it was last set via a call to
  // UniformXv().
  struct Uniform {
    Uniform() : location(-1), data(NULL) {}

    int location;
    UniformInfo info;
    void* data;
  };

  explicit Program(nb::scoped_ptr<ProgramImpl> impl);

  // Attaches the specified shader to either this program's vertex or fragment
  // shader slot, depending on the shader type.  If a shader of the given
  // type is already attached, this method does nothing and returns false,
  // otherwise the shader is attached and true is returned.
  bool AttachShader(const nb::scoped_refptr<Shader>& shader);

  // Links the vertex and fragment shaders, making the shader usable.
  void Link();

  bool linked() const { return link_results_.success; }

  void BindAttribLocation(GLuint index, const GLchar* name);

  ProgramImpl* impl() const { return impl_.get(); }

  GLenum GetProgramiv(GLenum pname, GLint* params);
  void GetProgramInfoLog(GLsizei bufsize, GLsizei* length, GLchar* infolog);

  GLint GetUniformLocation(const GLchar* name);
  GLenum Uniformiv(GLint location,
                   GLsizei count,
                   GLsizei elem_size,
                   const GLint* v);
  GLenum Uniformfv(GLint location,
                   GLsizei count,
                   GLsizei elem_size,
                   const GLfloat* v);
  GLenum UniformMatrixfv(GLint location,
                         GLsizei count,
                         GLsizei dim_size,
                         const GLfloat* value);

  // Returns the current list of all uniforms that have been targeted by
  // UniformXv() calls since the last call to Link().
  const std::vector<Uniform> uniforms() const { return uniforms_; }

 private:
  typedef std::map<unsigned int, std::string> BoundAttributes;
  friend class nb::RefCountedThreadSafe<Program>;
  ~Program() {}

  // Returns a U
  Uniform* FindOrMakeUniform(int location);
  void ClearUniforms();
  GLenum UpdateUniform(GLint location,
                       GLsizei count,
                       GLsizei elem_size,
                       const void* v,
                       UniformInfo::Type type);

  nb::scoped_ptr<ProgramImpl> impl_;

  nb::scoped_refptr<Shader> vertex_shader_;
  nb::scoped_refptr<Shader> fragment_shader_;

  // We explicitly reference the last-linked vertex and fragment shaders to
  // ensure that they stay valid as long as they're linked.
  nb::scoped_refptr<Shader> linked_vertex_shader_;
  nb::scoped_refptr<Shader> linked_fragment_shader_;

  // Stores the value that will be returned when glGetProgramiv(GL_LINK_STATUS)
  // or glGetProgramiv(GL_INFO_LOG_LENGTH) is called.
  ProgramImpl::LinkResults link_results_;

  // Constructed by glBindAttribLocation(), this maps generic vertex attribute
  // indices to attribute names, and applies when and after a program is linked.
  BoundAttributes bound_attrib_locations_;

  // The list of all uniforms that have been assigned values through
  // the UniformXv() methods.
  std::vector<Uniform> uniforms_;

  // Keeps track of all uniform locations that have been returned by a call
  // to GetUniformLocation() above.  These may or may not have had values
  // assigned to them since a Link().
  std::vector<int> active_uniform_locations_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_PROGRAM_H_
