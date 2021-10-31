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

// The blur rounded rect is split into top and bottom halves.
// The "start" values represent (left_start.xy, right_start.xy).
// The "scale" values represent (left_radius.x, 1 / left_radius.y,
//   right_radius.x, 1 / right_radius.y). The sign of the scale value helps
//   to translate between position and corner offset values, where the corner
//   offset is positive if the position is inside the rounded corner.
uniform vec4 u_blur_start_top;
uniform vec4 u_blur_start_bottom;
uniform vec4 u_blur_scale_top;
uniform vec4 u_blur_scale_bottom;

// The blur extent specifies (blur_size, min_rect_y, max_rect_y, center_rect_y).
uniform vec4 u_blur_extent;

// The scale_add uniform is used to switch the shader between generating
// outset shadows and inset shadows. It impacts the shadow gradient and
// scissor behavior. Use (1, 0) to get an outset shadow with the provided
// scissor rect behaving as an exclusive scissor, and (-1, 1) to get an
// inset shadow with scissor rect behaving as an inclusive scissor.
uniform vec2 u_scale_add;

uniform vec4 u_color;

varying vec2 v_offset;
varying vec4 v_rcorner;

// Return 0 if the given position is inside the rounded corner, or scale
// towards 1 as it goes outside a 1-pixel anti-aliasing border.
// |rcorner| is a vec4 representing (scaled.xy, 1 / radius.xy) with scaled.xy
//   representing the offset of the current position in terms of radius.xy
//   (i.e. offset.xy / radius.xy). The scaled.xy values can be negative if the
//   current position is outside the corner start.
float IsOutsideRCorner(vec4 rcorner) {
  // Estimate the distance to an implicit function using
  //   dist = f(x,y) / length(gradient(f(x,y)))
  // For an ellipse, f(x,y) = x^2 / a^2 + y^2 / b^2 - 1.
  highp vec2 scaled = max(rcorner.xy, 0.0);
  highp float implicit = dot(scaled, scaled) - 1.0;

  // NOTE: To accommodate large radius values using mediump floats, rcorner.zw
  //   was scaled by kRCornerGradientScale in the vertex attribute data.
  //   Multiply inv_gradient by kRCornerGradientScale to undo that scaling.
  const highp float kRCornerGradientScale = 16.0;
  highp vec2 gradient = 2.0 * scaled * rcorner.zw;
  highp float inv_gradient = kRCornerGradientScale *
      inversesqrt(max(dot(gradient, gradient), 0.0001));

  return clamp(0.5 + implicit * inv_gradient, 0.0, 1.0);
}

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

float GetXBlur(float x, float y) {
  // Solve for X of the rounded corners at the given Y based on the equation
  // for an ellipse: x^2 / a^2 + y^2 / b^2 = 1.
  vec4 corner_start =
      (y < u_blur_extent.w) ? u_blur_start_top : u_blur_start_bottom;
  vec4 corner_scale =
      (y < u_blur_extent.w) ? u_blur_scale_top : u_blur_scale_bottom;
  vec2 scaled = clamp((y - corner_start.yw) * corner_scale.yw, 0.0, 1.0);
  vec2 root = sqrt(1.0 - scaled * scaled);
  vec2 extent_x = corner_start.xz + corner_scale.xz * root;

  // Get the integral over the interval occupied by the rectangle.
  return GaussianIntegral(extent_x - x);
}

float GetBlur(vec2 pos) {
  // Approximate the 2D gaussian filter using numerical integration. Sample
  // points between the y extents of the rectangle.
  float low = clamp(pos.y - u_blur_extent.x, u_blur_extent.y, u_blur_extent.z);
  float high = clamp(pos.y + u_blur_extent.x, u_blur_extent.y, u_blur_extent.z);

  // Use the Gauss–Legendre quadrature with 6 points to numerically integrate.
  // Using fewer samples will show artifacts with elliptical corners that are
  // likely to be used.
  const vec3 kStepScale1 = vec3(-0.932470, -0.661209, -0.238619);
  const vec3 kStepScale2 = vec3( 0.932470,  0.661209,  0.238619);
  const vec3 kWeight = vec3(0.171324, 0.360762, 0.467914);

  float half_size = (high - low) * 0.5;
  float middle = (high + low) * 0.5;
  vec3 weight = half_size * kWeight;
  vec3 pos1 = middle + half_size * kStepScale1;
  vec3 pos2 = middle + half_size * kStepScale2;
  vec3 offset1 = pos1 - pos.yyy;
  vec3 offset2 = pos2 - pos.yyy;

  // The integral along the x-axis is computed. The integral along the y-axis
  // is roughly approximated. To get the 2D filter, multiply the two integrals.
  // Visual artifacts appear when the computed integrals along the x-axis
  // change rapidly between samples (e.g. elliptical corners that are much
  // wider than they are tall).
  vec3 xblur1 = vec3(GetXBlur(pos.x, pos1.x),
                     GetXBlur(pos.x, pos1.y),
                     GetXBlur(pos.x, pos1.z));
  vec3 xblur2 = vec3(GetXBlur(pos.x, pos2.x),
                     GetXBlur(pos.x, pos2.y),
                     GetXBlur(pos.x, pos2.z));
  vec3 yblur1 = exp(-offset1 * offset1) * weight;
  vec3 yblur2 = exp(-offset2 * offset2) * weight;

  // Since each yblur value should be normalized by kNormalizeGaussian, just
  // scale the sum by it.
  const float kNormalizeGaussian = 0.564189584;  // 1 / sqrt(pi)
  return (dot(xblur1, yblur1) + dot(xblur2, yblur2)) * kNormalizeGaussian;
}

void main() {
  float scissor_scale =
      IsOutsideRCorner(v_rcorner) * u_scale_add.x + u_scale_add.y;
  float blur_scale = GetBlur(v_offset) * u_scale_add.x + u_scale_add.y;
  gl_FragColor = u_color * (blur_scale * scissor_scale);
}
