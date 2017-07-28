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

precision mediump float;
uniform vec2 u_blur_radius;
uniform vec2 u_scale_add;

// Adjust the sigma input value to tweak the blur output so that it better
// matches the reference. This is usually only needed for very large sigmas.
uniform vec2 u_sigma_tweak;

varying vec2 v_offset;    // Relative to the blur center.
varying vec4 v_color;

void main() {
  // Distance from the blur radius.
  // Both v_offset and u_blur_radius are expressed in terms of the
  //   blur sigma.
  vec2 pos = abs(v_offset) - u_blur_radius + u_sigma_tweak;
  vec2 pos2 = pos * pos;
  vec2 pos3 = pos2 * pos;
  vec4 posx = vec4(1.0, pos.x, pos2.x, pos3.x);
  vec4 posy = vec4(1.0, pos.y, pos2.y, pos3.y);

  // Approximation of the gaussian integral from [x, +inf).
  // http://stereopsis.com/shadowrect/
  const vec4 klower = vec4(0.4375, -1.125, -0.75, -0.1666666);
  const vec4 kmiddle = vec4(0.5, -0.75, 0.0, 0.3333333);
  const vec4 kupper = vec4(0.5625, -1.125, 0.75, -0.1666666);

  float gaussx = 0.0;
  if (pos.x < -1.5) {
    gaussx = 1.0;
  } else if (pos.x < -0.5) {
    gaussx = dot(posx, klower);
  } else if (pos.x < 0.5) {
    gaussx = dot(posx, kmiddle);
  } else if (pos.x < 1.5) {
    gaussx = dot(posx, kupper);
  }

  float gaussy = 0.0;
  if (pos.y < -1.5) {
    gaussy = 1.0;
  } else if (pos.y < -0.5) {
    gaussy = dot(posy, klower);
  } else if (pos.y < 0.5) {
    gaussy = dot(posy, kmiddle);
  } else if (pos.y < 1.5) {
    gaussy = dot(posy, kupper);
  }

  float alpha_scale = gaussx * gaussy * u_scale_add.x + u_scale_add.y;
  gl_FragColor = v_color * alpha_scale;
}
