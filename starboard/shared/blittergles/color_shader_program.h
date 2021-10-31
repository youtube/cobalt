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

#ifndef STARBOARD_SHARED_BLITTERGLES_COLOR_SHADER_PROGRAM_H_
#define STARBOARD_SHARED_BLITTERGLES_COLOR_SHADER_PROGRAM_H_

#include "starboard/blitter.h"
#include "starboard/shared/blittergles/shader_program.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace blittergles {

class ColorShaderProgram : public ShaderProgram {
 public:
  ColorShaderProgram();

  // Method that draws a colored rectangle on the render_target. Color is
  // specified by the float array containing R G B A in that order.
  bool Draw(SbBlitterRenderTarget render_target,
            float (&color_rgba)[4],
            SbBlitterRect rect) const;

  // Method that draws a 1x1 transparent rectangle and disables leak sanitizer
  // around glDrawArrays().
  void DummyDraw(SbBlitterRenderTarget render_target) const;

 private:
  // Location of the shader attribute "a_position" for the color shader.
  static const int kPositionAttribute = 0;

  // Location of the shader attribute "a_color" for the color shader.
  static const int kColorAttribute = 2;
};

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_BLITTERGLES_COLOR_SHADER_PROGRAM_H_
