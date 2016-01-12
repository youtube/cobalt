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

#include "glimp/gles/program_impl.h"
#include "glimp/gles/shader.h"
#include "glimp/nb/ref_counted.h"
#include "glimp/nb/scoped_ptr.h"

namespace glimp {
namespace gles {

class Program : public nb::RefCountedThreadSafe<Program> {
 public:
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

  ProgramImpl* impl() { return impl_.get(); }
  const ProgramImpl* impl() const { return impl_.get(); }

  GLenum GetProgramiv(GLenum pname, GLint* params);
  void GetProgramInfoLog(GLsizei bufsize, GLsizei* length, GLchar* infolog);

 private:
  typedef std::map<unsigned int, std::string> BoundAttributes;
  friend class nb::RefCountedThreadSafe<Program>;
  ~Program() {}

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
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_PROGRAM_H_
