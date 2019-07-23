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

#ifndef STARBOARD_SHARED_BLITTERGLES_BLIT_SHADER_PROGRAM_H_
#define STARBOARD_SHARED_BLITTERGLES_BLIT_SHADER_PROGRAM_H_

#include <GLES2/gl2.h>

#include "starboard/blitter.h"
#include "starboard/shared/blittergles/shader_program.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace blittergles {

class BlitShaderProgram : public ShaderProgram {
 public:
  BlitShaderProgram();

  bool Draw(SbBlitterContext context,
            SbBlitterSurface surface,
            SbBlitterRect src_rect,
            SbBlitterRect dst_rect) const;

 private:
  // Location of the shader uniform that controls the texcoord clamping.
  // Clamping is done so the fragment shader doesn't sample beyond the specified
  // src_rect.
  int clamp_uniform_;

  // Location of the shader uniform that determines the color matrix transform
  // to apply.
  int color_matrix_uniform_;
};

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_BLITTERGLES_BLIT_SHADER_PROGRAM_H_
