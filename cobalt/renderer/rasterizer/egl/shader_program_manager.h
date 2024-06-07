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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_SHADER_PROGRAM_MANAGER_H_
#define COBALT_RENDERER_RASTERIZER_EGL_SHADER_PROGRAM_MANAGER_H_

#include <unordered_map>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/type_id.h"
#include "cobalt/renderer/rasterizer/egl/shader_program.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// This class caches shader programs. This is not thread-safe, and the caller
// is expected to ensure access to these shader programs occurs from a single
// context.
class ShaderProgramManager {
 public:
  ShaderProgramManager();
  ~ShaderProgramManager();

  // Get the specified shader program from the cache or compile the program if
  // it wasn't cached.
  // NOTE: If a new program needed to be compiled, then the current
  // glUseProgram state may be altered as part of the program initialization.
  template <typename ShaderProgramType>
  void GetProgram(ShaderProgramType** out_program);

 protected:
  ShaderProgramBase* FindProgram(base::TypeId program_type_id);
  void AddProgram(base::TypeId program_type_id, ShaderProgramBase* program);

  template <typename VertextShaderT, typename FragmentShaderT>
  void Preload();

  typedef std::unordered_map<base::TypeId, ShaderProgramBase*,
                             BASE_HASH_NAMESPACE::hash<base::TypeId>>
      ProgramMap;
  ProgramMap program_map_;
};

template <typename ShaderProgramType>
inline void ShaderProgramManager::GetProgram(ShaderProgramType** out_program) {
  base::TypeId program_type_id = base::GetTypeId<ShaderProgramType>();
  ShaderProgramBase* program = FindProgram(program_type_id);
  if (program == NULL) {
    program = new ShaderProgramType();
    AddProgram(program_type_id, program);
  }
  *out_program = base::polymorphic_downcast<ShaderProgramType*>(program);
}

template <typename VertextShaderT, typename FragmentShaderT>
inline void ShaderProgramManager::Preload() {
  ShaderProgram<VertextShaderT, FragmentShaderT>* program;
  GetProgram(&program);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_SHADER_PROGRAM_MANAGER_H_
