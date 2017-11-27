// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_SHADER_PROGRAM_H_
#define COBALT_RENDERER_RASTERIZER_EGL_SHADER_PROGRAM_H_

#include "base/compiler_specific.h"
#include "cobalt/base/type_id.h"
#include "cobalt/renderer/rasterizer/egl/shader_base.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

class ShaderProgramManager;

// ShaderProgramBase represents a shader program with vertex and fragment
// shaders attached. This should be instantiated via the template ShaderProgram
// class.
class ShaderProgramBase {
 public:
  GLuint GetHandle() const { return handle_; }

 protected:
  // Shader programs should only be created and destroyed by the
  // ShaderProgramManager.
  friend class ShaderProgramManager;

  ShaderProgramBase();
  virtual ~ShaderProgramBase();
  void Create(ShaderBase* vertex_shader, ShaderBase* fragment_shader);
  void Destroy(ShaderBase* vertex_shader, ShaderBase* fragment_shader);

 private:
  GLuint handle_;
};

// This template ShaderProgram class represents a shader program with specific
// vertex and fragment shaders. The shaders must inherit from ShaderBase.
template<typename VertexShader, typename FragmentShader>
class ShaderProgram : public ShaderProgramBase {
 public:
  const VertexShader& GetVertexShader() const {
    return vertex_shader_;
  }
  const FragmentShader& GetFragmentShader() const {
    return fragment_shader_;
  }
  static base::TypeId GetTypeId() {
    return base::GetTypeId<ShaderProgram<VertexShader, FragmentShader> >();
  }

 private:
  // Shader programs should only be created and destroyed by the
  // ShaderProgramManager.
  friend class ShaderProgramManager;

  ShaderProgram() {
    Create(&vertex_shader_, &fragment_shader_);
  }
  ~ShaderProgram() override { Destroy(&vertex_shader_, &fragment_shader_); }

  VertexShader vertex_shader_;
  FragmentShader fragment_shader_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_SHADER_PROGRAM_H_
