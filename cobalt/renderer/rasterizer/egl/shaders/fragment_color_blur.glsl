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

precision mediump float;

uniform vec4 u_color;
uniform vec4 u_blur_rect;
uniform vec2 u_scale_add;

varying vec2 v_offset;

#include "function_gaussian_integral.inc"

void main() {
  // Get the integral over the interval occupied by the rectangle. Both
  // v_offset and u_blur_rect are already scaled for the integral function.
  float integral = GaussianIntegral(u_blur_rect.xz - v_offset.xx) *
                   GaussianIntegral(u_blur_rect.yw - v_offset.yy);
  float blur_scale = integral * u_scale_add.x + u_scale_add.y;
  gl_FragColor = u_color * blur_scale;
}
