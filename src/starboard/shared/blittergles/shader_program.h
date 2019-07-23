// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_BLITTERGLES_SHADER_PROGRAM_H_
#define STARBOARD_SHARED_BLITTERGLES_SHADER_PROGRAM_H_

#include <GLES2/gl2.h>

#include "starboard/blitter.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace blittergles {

class ShaderProgram {
 protected:
  ~ShaderProgram();

  void InitializeShaders(const char* vertex_shader_source,
                         const char* fragment_shader_source);

  GLuint GetProgramHandle() const { return program_handle_; }

  // Fills the given vertices array with the blitter-space rect coordinates
  // translated into normalized device coordinates (NDC).
  static const void SetNDC(SbBlitterRect rect,
                           int width,
                           int height,
                           float (&vertex_data)[8]);

  // Fills the given vertices array with the blitter-space rect coordinates
  // translated into texture uv coordinates.
  static const void SetTexCoords(SbBlitterRect rect,
                                 int width,
                                 int height,
                                 float (&vertex_data)[8]);

 private:
  // Represents the connection to the shader program.
  GLuint program_handle_;

  GLuint vertex_shader_;

  GLuint fragment_shader_;

  void InitializeShader(GLuint shader_handle, const char* shader_source);
};

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_BLITTERGLES_SHADER_PROGRAM_H_
