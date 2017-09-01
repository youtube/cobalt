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

// The rounded spread rect is represented in a way to optimize calculation of
// the extents. Each element of a vec4 represents a corner's value -- order
// is top left, top right, bottom left, bottom right. Extents for each corner
// can be calculated as:
//   extents_x = start_x + radius_x * sqrt(1 - scaled_y^2) where
//   scaled_y = clamp((pos.yyyy - start_y) * scale_y, 0.0, 1.0)
// To simplify handling left vs right and top vs bottom corners, the sign of
// scale_y and radius_x handles negation as needed.
uniform vec4 u_spread_start_x;
uniform vec4 u_spread_start_y;
uniform vec4 u_spread_scale_y;
uniform vec4 u_spread_radius_x;

// The blur extent specifies (3 * sigma, min_rect_y, max_rect_y). This is used
// to clamp the interval over which integration should be evaluated.
uniform vec3 u_blur_extent;

// The gaussian scale uniform is used to simplify calculation of the gaussian
// function at a particular point.
uniform vec2 u_gaussian_scale;

// The scale_add uniform is used to switch the shader between generating
// outset shadows and inset shadows. It impacts the shadow gradient and
// scissor behavior. Use (1, 0) to get an outset shadow with the provided
// scissor rect behaving as an exclusive scissor, and (-1, 1) to get an
// inset shadow with scissor rect behaving as an inclusive scissor.
uniform vec2 u_scale_add;

uniform vec4 u_color;

// Blur calculations happen in terms in sigma distances. Use sigma_scale to
// translate pixel distances into sigma distances.
uniform vec2 u_sigma_scale;

varying vec2 v_offset;
varying vec4 v_rcorner;

#include "function_is_outside_rcorner.inc"
#include "function_gaussian_integral.inc"

vec2 GetXExtents(float y) {
  // Use x^2 / a^2 + y^2 / b^2 = 1 to solve for the x value of each rounded
  // corner at the given y.
  vec4 scaled = clamp((y - u_spread_start_y) * u_spread_scale_y, 0.0, 1.0);
  vec4 root = sqrt(1.0 - scaled * scaled);
  vec4 extent = u_spread_start_x + u_spread_radius_x * root;

  // If the y value was before a corner started, then the calculated extent
  // would equal the unrounded rectangle's extents (since negative values were
  // clamped to 0 in the above calculation). So smaller extents (i.e. extents
  // closer to the rectangle center), represent the relevant corners' extents.
  return vec2(max(extent.x, extent.z), min(extent.y, extent.w));
}

float GetXBlur(float x, float y) {
  // Get the integral over the interval occupied by the rectangle.
  vec2 pos = (GetXExtents(y) - x) * u_sigma_scale;
  return GaussianIntegral(pos);
}

vec3 GetGaussian(vec3 offset) {
  // Evaluate the gaussian at the given offsets.
  return exp(offset * offset * u_gaussian_scale.x);
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
  vec3 yblur1 = GetGaussian(offset1) * weight;
  vec3 yblur2 = GetGaussian(offset2) * weight;

  // Since each yblur value should be scaled by u_gaussian_scale.y, save some
  // cycles and multiply the sum by it.
  return (dot(xblur1, yblur1) + dot(xblur2, yblur2)) * u_gaussian_scale.y;
}

void main() {
  float scissor_scale =
      IsOutsideRCorner(v_rcorner) * u_scale_add.x + u_scale_add.y;
  float blur_scale = GetBlur(v_offset) * u_scale_add.x + u_scale_add.y;
  gl_FragColor = u_color * (blur_scale * scissor_scale);
}
