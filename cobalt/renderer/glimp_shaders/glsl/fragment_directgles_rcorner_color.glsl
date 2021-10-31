// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

precision mediump float;

uniform vec4 u_color;
varying vec4 v_rcorner;

// Return 0 if the given position is inside the rounded corner, or scale
// towards 1 as it goes outside a 1-pixel anti-aliasing border.
// |rcorner| is a vec4 representing (scaled.xy, 1 / radius.xy) with scaled.xy
//   representing the offset of the current position in terms of radius.xy
//   (i.e. offset.xy / radius.xy). The scaled.xy values can be negative if the
//   current position is outside the corner start.
float IsOutsideRCorner(vec4 rcorner) {
  // Estimate the distance to an implicit function using
  //   dist = f(x,y) / length(gradient(f(x,y)))
  // For an ellipse, f(x,y) = x^2 / a^2 + y^2 / b^2 - 1.
  highp vec2 scaled = max(rcorner.xy, 0.0);
  highp float implicit = dot(scaled, scaled) - 1.0;

  // NOTE: To accommodate large radius values using mediump floats, rcorner.zw
  //   was scaled by kRCornerGradientScale in the vertex attribute data.
  //   Multiply inv_gradient by kRCornerGradientScale to undo that scaling.
  const highp float kRCornerGradientScale = 16.0;
  highp vec2 gradient = 2.0 * scaled * rcorner.zw;
  highp float inv_gradient = kRCornerGradientScale *
      inversesqrt(max(dot(gradient, gradient), 0.0001));

  return clamp(0.5 + implicit * inv_gradient, 0.0, 1.0);
}

void main() {
  float scale = IsOutsideRCorner(v_rcorner);
  gl_FragColor = u_color * (1.0 - scale);
}
