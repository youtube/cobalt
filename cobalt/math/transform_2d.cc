// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/math/transform_2d.h"

#include <algorithm>
#include <cmath>

namespace cobalt {
namespace math {

Matrix3F TranslateMatrix(float x, float y) {
  return Matrix3F::FromValues(1.0f, 0, x, 0, 1.0f, y, 0, 0, 1.0f);
}

Matrix3F ScaleMatrix(float x_scale, float y_scale) {
  return Matrix3F::FromValues(x_scale, 0, 0, 0, y_scale, 0, 0, 0, 1.0f);
}

Matrix3F ScaleMatrix(float scale) { return ScaleMatrix(scale, scale); }

Matrix3F RotateMatrix(float counter_clockwise_angle_in_radians) {
  float sin_theta = sin(counter_clockwise_angle_in_radians);
  float cos_theta = cos(counter_clockwise_angle_in_radians);

  // Rotation matrix for a space where up is represented by negative y.
  return Matrix3F::FromValues(cos_theta, sin_theta, 0, -sin_theta, cos_theta, 0,
                              0, 0, 1.0f);
}

Vector2dF GetScale2d(const Matrix3F& transform) {
  float m00 = transform(0, 0);
  float m01 = transform(0, 1);
  float m10 = transform(1, 0);
  float m11 = transform(1, 1);
  return Vector2dF(std::sqrt(m00 * m00 + m10 * m10),
                   std::sqrt(m01 * m01 + m11 * m11));
}

bool IsOnlyScaleAndTranslate(const Matrix3F& transform) {
  const float kEpsilon = 0.0001f;
  return std::abs(transform(0, 1)) < kEpsilon &&
         std::abs(transform(1, 0)) < kEpsilon &&
         std::abs(transform(2, 0)) < kEpsilon &&
         std::abs(transform(2, 1)) < kEpsilon &&
         std::abs(transform(2, 2) - 1.0f) < kEpsilon;
}

}  // namespace math
}  // namespace cobalt
