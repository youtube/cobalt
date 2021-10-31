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
varying vec2 v_texcoord;

uniform sampler2D u_texture_y;
uniform sampler2D u_texture_u;
uniform sampler2D u_texture_v;
uniform mat4 u_color_transform_matrix;

#pragma array u_texture(u_texture_y, u_texture_u, u_texture_v);

vec4 GetRgba() {
  float y = texture2D(u_texture_y, v_texcoord).a;
  float u = texture2D(u_texture_u, v_texcoord).a;
  float v = texture2D(u_texture_v, v_texcoord).a;
  vec4 rgba = u_color_transform_matrix * vec4(y, u, v, 1);

  return clamp(rgba, vec4(0, 0, 0, 0), vec4(1, 1, 1, 1));
}

void main() {
  gl_FragColor = GetRgba();
}
