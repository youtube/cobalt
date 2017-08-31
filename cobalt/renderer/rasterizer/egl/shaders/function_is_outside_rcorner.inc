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

// Return 0 if the given position is inside the rounded corner, or scale
// towards 1 as it goes outside a 1-pixel anti-aliasing border.
// |corner| is a vec4 representing (start.xy, 1 / radius.xy) with the sign of
//   the radius component used to translate offset into the first quadrant.
float IsOutsideRCorner(vec2 pos, vec4 corner) {
  // Estimate the distance to an implicit function using
  //   dist = f(x,y) / length(gradient(f(x,y)))
  // For an ellipse, f(x,y) = x^2 / a^2 + y^2 / b^2 - 1.
  vec2 scaled = max((pos - corner.xy) * corner.zw, 0.0);
  float implicit = dot(scaled, scaled) - 1.0;
  vec2 gradient = 2.0 * scaled * corner.zw;
  float inv_gradient = inversesqrt(max(dot(gradient, gradient), 0.0001));
  return clamp(0.5 + implicit * inv_gradient, 0.0, 1.0);
}
