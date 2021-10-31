// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

uniform vec4 u_texcoord_clamp_y;
uniform vec4 u_texcoord_clamp_u;
uniform vec4 u_texcoord_clamp_v;
uniform sampler2D u_texture_y;
uniform sampler2D u_texture_u;
uniform sampler2D u_texture_v;
uniform mat4 u_color_transform_matrix;

#pragma array u_texcoord_clamp(u_texcoord_clamp_y, u_texcoord_clamp_u, u_texcoord_clamp_v);
#pragma array u_texture(u_texture_y, u_texture_u, u_texture_v);

vec4 GetRgba() {
  float y = texture2D(u_texture_y,
                      clamp(v_texcoord, u_texcoord_clamp_y.xy, u_texcoord_clamp_y.zw)).a;
  float u = texture2D(u_texture_u,
                      clamp(v_texcoord, u_texcoord_clamp_u.xy, u_texcoord_clamp_u.zw)).a;
  float v = texture2D(u_texture_v,
                      clamp(v_texcoord, u_texcoord_clamp_v.xy, u_texcoord_clamp_v.zw)).a;
  vec4 rgba = u_color_transform_matrix * vec4(y, u, v, 1);

  return clamp(rgba, vec4(0, 0, 0, 0), vec4(1, 1, 1, 1));
}
