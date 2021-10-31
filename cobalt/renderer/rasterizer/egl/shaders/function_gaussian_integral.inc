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

// Calculate the normalized gaussian integral from (pos.x * k, pos.y * k)
// where k = sqrt(2) * sigma and pos.x <= pos.y. This is just a 1D filter --
// the pos.x and pos.y values are expected to be on the same axis.
float GaussianIntegral(vec2 pos) {
  // Approximation of the error function.
  // For x >= 0,
  //   erf(x) = 1 - 1 / (1 + k1 * x + k2 * x^2 + k3 * x^3 + k4 * x^4)^4
  //   where k1 = 0.278393, k2 = 0.230389, k3 = 0.000972, k4 = 0.078108.
  // For y < 0,
  //   erf(y) = -erf(-y).
  vec2 s = sign(pos);
  vec2 a = abs(pos);
  vec2 t = 1.0 +
           (0.278393 + (0.230389 + (0.000972 + 0.078108 * a) * a) * a) * a;
  vec2 t2 = t * t;
  vec2 erf = s - s / (t2 * t2);

  // erf(x) = the integral of the normalized gaussian from [-x * k, x * k],
  // where k = sqrt(2) * sigma. Find the integral from (pos.x * k, pos.y * k).
  return dot(erf, vec2(-0.5, 0.5));
}
