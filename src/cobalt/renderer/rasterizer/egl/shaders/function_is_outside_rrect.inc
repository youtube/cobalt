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

// Return 0 if the given point is inside the rounded rect, or scale towards 1
// as it goes outside a 1-pixel anti-aliasing border.
// |rect| represents (min.xy, max.xy) of the encompassing rectangle.
// |corners| is a matrix with each vec4 representing (start.xy, radius.xy) of
//   a corner. The order is top left, top right, bottom left, bottom right.
float IsOutsideRRect(vec2 point, vec4 rect, mat4 corners) {
  vec4 select_corner = vec4(
      step(point.x, corners[0].x) * step(point.y, corners[0].y),
      step(corners[1].x, point.x) * step(point.y, corners[1].y),
      step(point.x, corners[2].x) * step(corners[2].y, point.y),
      step(corners[3].x, point.x) * step(corners[3].y, point.y));
  if (dot(select_corner, vec4(1.0)) > 0.5) {
    // Estimate the amount of anti-aliasing that should be used by comparing
    // x^2 / a^2 + y^2 / b^2 for the ellipse and ellipse + 1 pixel.
    vec4 corner = corners * select_corner;
    vec2 pixel_offset = point - corner.xy;

    if (abs(corner.z - corner.w) < 0.1) {
      // This is a square or round corner.
      return clamp(length(pixel_offset) - corner.z, 0.0, 1.0);
    }

    vec2 offset_min = pixel_offset / corner.zw;
    vec2 offset_max = pixel_offset / (corner.zw + vec2(1.0));
    float result_min = dot(offset_min, offset_min);
    float result_max = dot(offset_max, offset_max);

    // Return 1.0 if outside, or interpolate if in the border, or 0 if inside.
    return (result_max >= 1.0) ? 1.0 :
        max(result_min - 1.0, 0.0) / (result_min - result_max);
  }

  return clamp(rect.x - point.x, 0.0, 1.0) +
         clamp(point.x - rect.z, 0.0, 1.0) +
         clamp(rect.y - point.y, 0.0, 1.0) +
         clamp(point.y - rect.w, 0.0, 1.0);
}
