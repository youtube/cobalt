/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef GLIMP_GLES_BLEND_STATE_H_
#define GLIMP_GLES_BLEND_STATE_H_

namespace glimp {
namespace gles {

// Describes GL blend state, which can be modified via commands like
// glBlendFunc().
//   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glBlendFunc.xml
struct BlendState {
  enum Factor {
    kFactorZero,
    kFactorOne,
    kFactorSrcColor,
    kFactorOneMinusSrcColor,
    kFactorDstColor,
    kFactorOneMinusDstColor,
    kFactorSrcAlpha,
    kFactorOneMinusSrcAlpha,
    kFactorDstAlpha,
    kFactorOneMinusDstAlpha,
    kFactorConstantColor,
    kFactorOneMinusConstantColor,
    kFactorConstantAlpha,
    kFactorOneMinusConstantAlpha,
    kFactorSrcAlphaSaturate,
    kFactorInvalid,
  };

  // https://www.khronos.org/registry/OpenGL-Refpages/es2.0/xhtml/glBlendEquation.xml
  enum Equation {
    kEquationFuncAdd,
    kEquationFuncSubtract,
    kEquationFuncReverseSubtract,
    kEquationFuncInvalid,
  };

  // Setup the blend state with initial values specified by the specification.
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glBlendFunc.xml
  BlendState()
      : src_factor(kFactorOne),
        dst_factor(kFactorZero),
        enabled(false),
        equation(kEquationFuncAdd) {}

  Factor src_factor;
  Factor dst_factor;

  Equation equation;

  bool enabled;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_BLEND_STATE_H_
