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

// Return 0 if the current point is inside the rounded rect, or scale towards 1
// as it goes outside a 1-pixel anti-aliasing border.
float GetRRectScale(vec4 rect, mat4 corners) {
  vec4 select_corner = vec4(
      step(v_offset.x, corners[0].x) * step(v_offset.y, corners[0].y),
      step(corners[1].x, v_offset.x) * step(v_offset.y, corners[1].y),
      step(v_offset.x, corners[2].x) * step(corners[2].y, v_offset.y),
      step(corners[3].x, v_offset.x) * step(corners[3].y, v_offset.y));
  if (dot(select_corner, vec4(1.0)) > 0.5) {
    // Estimate the amount of anti-aliasing that should be used by comparing
    // x^2 / a^2 + y^2 / b^2 for the ellipse and ellipse + 1 pixel.
    vec4 corner = corners * select_corner;
    vec2 pixel_offset = v_offset - corner.xy;

    if (corner.z * corner.w < 0.1) {
      // This is a square corner.
      return min(length(pixel_offset), 1.0);
    }

    vec2 offset_min = pixel_offset / corner.zw;
    vec2 offset_max = pixel_offset / (corner.zw + vec2(1.0));
    float result_min = dot(offset_min, offset_min);
    float result_max = dot(offset_max, offset_max);

    // Return 1.0 if outside, or interpolate if in the border, or 0 if inside.
    return (result_max >= 1.0) ? 1.0 :
        max(result_min - 1.0, 0.0) / (result_min - result_max);
  }

  return clamp(rect.x - v_offset.x, 0.0, 1.0) +
         clamp(v_offset.x - rect.z, 0.0, 1.0) +
         clamp(rect.y - v_offset.y, 0.0, 1.0) +
         clamp(v_offset.y - rect.w, 0.0, 1.0);
}

void main() {
  float inner_scale = GetRRectScale(u_inner_rect, u_inner_corners);
  float outer_scale = GetRRectScale(u_outer_rect, u_outer_corners);
  gl_FragColor = v_color * inner_scale * (1.0 - outer_scale);
}
