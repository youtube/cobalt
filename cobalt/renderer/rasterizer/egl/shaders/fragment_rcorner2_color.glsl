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
varying vec4 v_rcorner_inner;
varying vec4 v_rcorner_outer;

#include "function_is_outside_rcorner.inc"

void main() {
  float inner_scale = IsOutsideRCorner(v_rcorner_inner);
  float outer_scale = 1.0 - IsOutsideRCorner(v_rcorner_outer);
  gl_FragColor = u_color * (inner_scale * outer_scale);
}
