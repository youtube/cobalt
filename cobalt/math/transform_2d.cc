/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/math/transform_2d.h"

#include <cmath>

namespace cobalt {
namespace math {

cobalt::math::Matrix3F TranslateMatrix(float x, float y) {
  return cobalt::math::Matrix3F::FromValues(
      1.0f, 0,    x,
      0,    1.0f, y,
      0,    0,    1.0f);
}

cobalt::math::Matrix3F ScaleMatrix(float x_scale, float y_scale) {
  return cobalt::math::Matrix3F::FromValues(
      x_scale, 0,       0,
      0,       y_scale, 0,
      0,       0,       1.0f);
}

cobalt::math::Matrix3F ScaleMatrix(float scale) {
  return ScaleMatrix(scale, scale);
}

cobalt::math::Matrix3F RotateMatrix(float counter_clockwise_angle_in_radians) {
  float sin_theta = sin(counter_clockwise_angle_in_radians);
  float cos_theta = cos(counter_clockwise_angle_in_radians);

  // Rotation matrix for a space where up is represented by negative y.
  return cobalt::math::Matrix3F::FromValues(
      cos_theta,  sin_theta,  0,
      -sin_theta, cos_theta,  0,
      0,          0,          1.0f);
}

}  // namespace math
}  // namespace cobalt
