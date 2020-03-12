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
varying vec4 v_color;
varying vec2 v_texcoord;

uniform vec4 u_texcoord_clamp_rgba;
uniform sampler2D u_texture_rgba;

#pragma array u_texcoord_clamp(u_texcoord_clamp_rgba);
#pragma array u_texture(u_texture_rgba);

vec4 GetRgba() {
  return texture2D(u_texture_rgba,
      clamp(v_texcoord, u_texcoord_clamp_rgba.xy, u_texcoord_clamp_rgba.zw));
}

void main() {
  gl_FragColor = v_color * GetRgba();
}
