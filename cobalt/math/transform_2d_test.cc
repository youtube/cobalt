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

#include <cmath>

#include "cobalt/math/transform_2d.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::math::Matrix3F;

TEST(Transform2DTest, Translation) {
  Matrix3F expected_value = Matrix3F::FromValues(
      1.0f, 0,    3.0f,
      0,    1.0f, 4.0f,
      0,    0,    1.0f);

  EXPECT_TRUE(expected_value.IsNear(
      cobalt::math::TranslateMatrix(3, 4), 0.00001f));
}

TEST(Transform2DTest, AnisotropicScale) {
  Matrix3F expected_value = Matrix3F::FromValues(
      3.0f, 0,    0,
      0,    4.0f, 0,
      0,    0,    1.0f);

  EXPECT_TRUE(expected_value.IsNear(
      cobalt::math::ScaleMatrix(3, 4), 0.00001f));
}

TEST(Transform2DTest, IsotropicScale) {
  Matrix3F expected_value = Matrix3F::FromValues(
      3.0f, 0,    0,
      0,    3.0f, 0,
      0,    0,    1.0f);

  EXPECT_TRUE(expected_value.IsNear(
      cobalt::math::ScaleMatrix(3), 0.00001f));
}

TEST(Transform2DTest, Rotation) {
  const float kTheta = static_cast<float>(M_PI) / 4.0f;

  // Note that we account for the fact that up is represented by negative y.
  Matrix3F expected_value = Matrix3F::FromValues(
      cos(kTheta),  sin(kTheta), 0,
      -sin(kTheta), cos(kTheta), 0,
      0,            0,           1.0f);

  EXPECT_TRUE(expected_value.IsNear(
      cobalt::math::RotateMatrix(kTheta), 0.00001f));
}
