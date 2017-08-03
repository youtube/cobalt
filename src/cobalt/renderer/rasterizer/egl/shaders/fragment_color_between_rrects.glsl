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

// A rounded rect is represented by a vec4 specifying (min.xy, max.xy), and a
// matrix of corners. Each vector in the matrix represents a corner (order:
// top left, top right, bottom left, bottom right). Each corner vec4 represents
// (start.xy, radius.xy).
uniform vec4 u_inner_rect;
uniform mat4 u_inner_corners;
uniform vec4 u_outer_rect;
uniform mat4 u_outer_corners;

varying vec2 v_offset;
varying vec4 v_color;

#include "function_is_outside_rrect.inc"

void main() {
  float inner_scale = IsOutsideRRect(v_offset, u_inner_rect, u_inner_corners);
  float outer_scale = IsOutsideRRect(v_offset, u_outer_rect, u_outer_corners);
  gl_FragColor = v_color * (inner_scale * (1.0 - outer_scale));
}
