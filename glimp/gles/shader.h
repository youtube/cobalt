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

#ifndef GLIMP_GLES_SHADER_H_
#define GLIMP_GLES_SHADER_H_

#include <GLES3/gl3.h>

#include <string>

#include "glimp/gles/shader_impl.h"
#include "nb/ref_counted.h"
#include "nb/scoped_ptr.h"

namespace glimp {
namespace gles {

class Shader : public nb::RefCountedThreadSafe<Shader> {
 public:
  Shader(nb::scoped_ptr<ShaderImpl> impl, GLenum type);

  GLenum type() const { return type_; }

  // Called when glShaderSource() is called.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glShaderSource.xml
  void ShaderSource(GLsizei count,
                    const GLchar* const* string,
                    const GLint* length);

  // Called when glCompileShader() is called.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glCompileShader.xml
  void CompileShader();

  // Called when glGetShaderiv() is called.  Returns GL_NO_ERROR if the query
  // was successful, otherwise returns the error GLenum that should be returned
  // to the caller.
  GLenum GetShaderiv(GLenum pname, GLint* params);
  void GetShaderInfoLog(GLsizei bufsize, GLsizei* length, GLchar* infolog);

  bool compiled() const { return compile_results_.success; }

  ShaderImpl* impl() const { return impl_.get(); }

 private:
  friend class nb::RefCountedThreadSafe<Shader>;
  ~Shader() {}

  nb::scoped_ptr<ShaderImpl> impl_;

  GLenum type_;

  std::string source_;

  // Keeps track of the success of the last CompileShader() call.  Can be
  // queried via glGetShaderiv() with the parameter GL_COMPILE_STATUS or
  // GL_INFO_LOG_LENGTH.
  ShaderImpl::CompileResults compile_results_;

  GLint compile_status_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_SHADER_H_
