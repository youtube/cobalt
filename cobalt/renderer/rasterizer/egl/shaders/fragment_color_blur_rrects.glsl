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

// A rounded rect is represented by a vec4 specifying (min.xy, max.xy)
// and a matrix of corners. Each vector in the matrix represents a corner
// (order: top left, top right, bottom left, bottom right). Each corner vec4
// represents (start.xy, radius.xy).
uniform vec4 u_scissor_rect;
uniform mat4 u_scissor_corners;
uniform vec4 u_spread_rect;
uniform mat4 u_spread_corners;

// The scale_add uniform is used to switch the shader between generating
// outset shadows and inset shadows. It impacts the shadow gradient and
// scissor behavior. Use (1, 0) to get an outset shadow with the provided
// scissor rect behaving as an exclusive scissor, and (-1, 1) to get an
// inset shadow with scissor rect behaving as an inclusive scissor.
uniform vec2 u_scale_add;

// Blur calculations happen in terms in sigma distances. Use sigma_scale to
// translate pixel distances into sigma distances.
uniform vec2 u_sigma_scale;

// Adjust the sigma input value to tweak the blur output so that it better
// matches the reference. This is usually only needed for very large sigmas.
uniform vec2 u_sigma_tweak;

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

// Calculate the distance from a point in the first quadrant to an ellipse
// centered at the origin.
//
// http://iquilezles.org/www/articles/ellipsedist/ellipsedist.htm
float GetEllipseDistance(vec2 p, vec2 ab) {
  if (abs(ab.x - ab.y) < 0.5) {
    return length(p) - ab.x;
  }

  if (p.x > p.y) {
    p = p.yx;
    ab = ab.yx;
  }

  float lr = 1.0 / (ab.y * ab.y - ab.x * ab.x);
  float m = ab.x * p.x * lr;
  float m2 = m * m;
  float n = ab.y * p.y * lr;
  float n2 = n * n;
  float c = (m2 + n2 - 1.0) * 0.333333333;
  float c3 = c * c * c;
  float q = c3 + m2 * n2 * 2.0;
  float d = c3 + m2 * n2;
  float g = m + m * n2;

  float co;

  if (d < 0.0) {
    float p = acos(q / c3) * 0.333333333;
    float s = cos(p);
    float t = sin(p) * 1.732050808;
    float rx = sqrt(-c * (s + t + 2.0) + m2);
    float ry = sqrt(-c * (s - t + 2.0) + m2);
    co = (ry + sign(lr) * rx + abs(g) / (rx * ry) - m) * 0.5;
  } else {
    float h = 2.0 * m * n * sqrt(d);
    float s = sign(q + h) * pow(abs(q + h), 0.333333333);
    float u = sign(q - h) * pow(abs(q - h), 0.333333333);
    float rx = -s - u - c * 4.0 + 2.0 * m2;
    float ry = (s - u) * 1.732050808;
    float rm = sqrt(rx * rx + ry * ry);
    float p = ry / sqrt(rm - rx);
    co = (p + 2.0 * g / rm - m) * 0.5;
  }

  float si = sqrt(1.0 - co * co);
  vec2 closest = vec2(ab.x * co, ab.y * si);
  return length(closest - p) * sign(p.y - closest.y);
}

// Get the x and y distances from the nearest edges of the rounded rect.
vec2 GetBlurPosition(vec4 rect, mat4 corners) {
  vec2 pos = max(rect.xy - v_offset, v_offset - rect.zw);

  vec4 select_corner = vec4(
      step(v_offset.x, corners[0].x) * step(v_offset.y, corners[0].y),
      step(corners[1].x, v_offset.x) * step(v_offset.y, corners[1].y),
      step(v_offset.x, corners[2].x) * step(corners[2].y, v_offset.y),
      step(corners[3].x, v_offset.x) * step(corners[3].y, v_offset.y));
  if (dot(select_corner, vec4(1.0)) > 0.5) {
    // Use distance from the closest point on the ellipse as the position
    // for blur calculations.
    vec4 corner = corners * select_corner;
    float dist = GetEllipseDistance(abs(v_offset - corner.xy), corner.zw);

    // Since the transition from non-corner to corner positions happens along
    // the ellipse's x or y axis, either pos.x = -corner.z or pos.y = -corner.w
    // when the transition happens. Keep one ordinate at a minimum so that
    // distance from the ellipse smoothly changes blur intensity.
    //
    // The return vec2 is used as blur = gi(x) * gi(y) where gi = the guassian
    // integral function. Swapping x and y has no impact on the final blur, so
    // distance can be substituted for one ordinate as long as the other takes
    // the right value. E.g. when pos = (-corner.z, 0), the point is on the top
    // center of the ellipse, so dist = 0. When pos = (0, -corner.w), dist = 0.
    // When pos = (-corner.z, -corner.w), the point is at the center of the
    // ellipse, and dist = max(-corner.z, -corner.w). So dist can be used as
    // long as the other ordinate is min(pos.x, pos.y).
    return vec2(dist, min(min(pos.x, pos.y), max(-corner.z, -corner.w)));
  }

  return pos;
}

void main() {
  float scissor_scale = GetRRectScale(u_scissor_rect, u_scissor_corners) *
                        u_scale_add.x + u_scale_add.y;

  vec2 pos = GetBlurPosition(u_spread_rect, u_spread_corners) *
             u_sigma_scale + u_sigma_tweak;
  vec2 pos2 = pos * pos;
  vec2 pos3 = pos2 * pos;
  vec4 posx = vec4(1.0, pos.x, pos2.x, pos3.x);
  vec4 posy = vec4(1.0, pos.y, pos2.y, pos3.y);

  // Approximation of the gaussian integral from [x, +inf).
  // http://stereopsis.com/shadowrect/
  const vec4 klower = vec4(0.4375, -1.125, -0.75, -0.1666666);
  const vec4 kmiddle = vec4(0.5, -0.75, 0.0, 0.3333333);
  const vec4 kupper = vec4(0.5625, -1.125, 0.75, -0.1666666);

  float gaussx = 0.0;
  if (pos.x < -1.5) {
    gaussx = 1.0;
  } else if (pos.x < -0.5) {
    gaussx = dot(posx, klower);
  } else if (pos.x < 0.5) {
    gaussx = dot(posx, kmiddle);
  } else if (pos.x < 1.5) {
    gaussx = dot(posx, kupper);
  }

  float gaussy = 0.0;
  if (pos.y < -1.5) {
    gaussy = 1.0;
  } else if (pos.y < -0.5) {
    gaussy = dot(posy, klower);
  } else if (pos.y < 0.5) {
    gaussy = dot(posy, kmiddle);
  } else if (pos.y < 1.5) {
    gaussy = dot(posy, kupper);
  }

  float alpha_scale = gaussx * gaussy * u_scale_add.x + u_scale_add.y;
  gl_FragColor = v_color * (alpha_scale * scissor_scale);
}
