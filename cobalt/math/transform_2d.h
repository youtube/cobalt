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

#ifndef MATH_TRANSFORM_2D_H_
#define MATH_TRANSFORM_2D_H_

#include "cobalt/math/matrix3_f.h"

namespace cobalt {
namespace math {

cobalt::math::Matrix3F TranslateMatrix(float x, float y);
cobalt::math::Matrix3F ScaleMatrix(float x_scale, float y_scale);
cobalt::math::Matrix3F ScaleMatrix(float scale);
cobalt::math::Matrix3F RotateMatrix(float counter_clockwise_angle_in_radians);

}  // namespace math
}  // namespace cobalt

#endif  // MATH_TRANSFORM_2D_H_
