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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/rasterizer/egl/shader_program_manager.h"

#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "egl/generated_shader_impl.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

ShaderProgramManager::ShaderProgramManager() {
  // These are shaders that get instantiated during video playback when the
  // user starts interacting with the transport controls. They are preloaded
  // to prevent UI-hiccups.
  // These shaders are generated from egl/generated_shader_impl.h
  Preload<ShaderVertexOffsetRcorner, ShaderFragmentColorBlurRrects>();
  Preload<ShaderVertexColorOffset, ShaderFragmentColorInclude>();
  Preload<ShaderVertexRcornerTexcoord, ShaderFragmentRcornerTexcoordColor>();
  Preload<ShaderVertexRcornerTexcoord,
          ShaderFragmentRcornerTexcoordColorYuv3>();
  Preload<ShaderVertexRcorner, ShaderFragmentRcornerColor>();
  Preload<ShaderVertexColorTexcoord, ShaderFragmentColorTexcoord>();
  Preload<ShaderVertexColorTexcoord, ShaderFragmentColorTexcoordYuv3>();
}

ShaderProgramManager::~ShaderProgramManager() {
  GL_CALL(glUseProgram(0));
  for (ProgramMap::iterator iter = program_map_.begin();
       iter != program_map_.end(); ++iter) {
    delete iter->second;
  }
}

ShaderProgramBase* ShaderProgramManager::FindProgram(
    base::TypeId program_type_id) {
  ProgramMap::iterator iter = program_map_.find(program_type_id);
  if (iter != program_map_.end()) {
    return iter->second;
  } else {
    return NULL;
  }
}

void ShaderProgramManager::AddProgram(base::TypeId program_type_id,
                                      ShaderProgramBase* program) {
  program_map_[program_type_id] = program;
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
