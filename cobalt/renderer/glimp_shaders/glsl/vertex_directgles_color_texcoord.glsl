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

uniform vec4 u_clip_adjustment;
uniform mat3 u_view_matrix;
attribute vec2 a_position;
attribute vec4 a_color;
attribute vec2 a_texcoord;
varying vec4 v_color;
varying vec2 v_texcoord;

void main() {
  vec3 pos2d = u_view_matrix * vec3(a_position, 1);
  gl_Position = vec4(pos2d.xy * u_clip_adjustment.xy +
                     u_clip_adjustment.zw, 0, pos2d.z);
  v_color = a_color;
  v_texcoord = a_texcoord;
}
