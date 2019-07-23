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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_SHADER_BASE_H_
#define COBALT_RENDERER_RASTERIZER_EGL_SHADER_BASE_H_

#include "cobalt/renderer/egl_and_gles.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

class ShaderProgramBase;

// ShaderBase represents vertex/fragment/etc. shaders. It provides methods
// to access GLES functionality that child classes can use. This should only
// be used as part of the ShaderProgram template class.
class ShaderBase {
 public:
  virtual ~ShaderBase() {}

  virtual const char* GetSource() const = 0;

 protected:
  void BindAttribLocation(GLuint program, GLuint location, const char* name);
  GLuint GetUniformLocation(GLuint program, const char* name);
  void SetTextureUnitForUniformSampler(GLuint uniform_location,
                                       GLenum texture_unit);

 private:
  friend class ShaderProgramBase;

  virtual void InitializePreLink(GLuint program) = 0;
  virtual void InitializePostLink(GLuint program) = 0;
  virtual void InitializePostUse() = 0;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_SHADER_BASE_H_
