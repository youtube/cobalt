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

#ifndef COBALT_MATH_TRANSFORM_2D_H_
#define COBALT_MATH_TRANSFORM_2D_H_

#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/vector2d_f.h"

namespace cobalt {
namespace math {

// Get a transform matrix with only the specified translation.
Matrix3F TranslateMatrix(float x, float y);
inline Matrix3F TranslateMatrix(const Vector2dF& translate) {
  return TranslateMatrix(translate.x(), translate.y());
}

// Get a transform matrix with only the specified scaling.
Matrix3F ScaleMatrix(float x_scale, float y_scale);
inline Matrix3F ScaleMatrix(const Vector2dF& scale) {
  return ScaleMatrix(scale.x(), scale.y());
}

// Get a transform matrix with only the specified uniform scaling.
Matrix3F ScaleMatrix(float scale);

// Get a transform matrix with only the specified rotation.
Matrix3F RotateMatrix(float counter_clockwise_angle_in_radians);

// Get the x-axis and y-axis scale factors of the given transform.
Vector2dF GetScale2d(const Matrix3F& transform);

// Determine if the given transform only scales and/or translates.
bool IsOnlyScaleAndTranslate(const Matrix3F& transform);

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_TRANSFORM_2D_H_
