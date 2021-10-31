// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/math/matrix_interpolation.h"
#include "cobalt/math/transform_2d.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace math {

namespace {
Matrix3F DecomposeAndRecompose(const Matrix3F& input) {
  return RecomposeMatrix(DecomposeMatrix(input));
}
}  // namespace

TEST(MatrixInterpolationTest, DecomposeAndRecomposeIsIdentityForXShear) {
  Matrix3F input(Matrix3F::FromValues(1, 1, 0, 0, 1, 0, 0, 0, 1));

  EXPECT_TRUE(input.IsNear(DecomposeAndRecompose(input), 0.001f));
}

TEST(MatrixInterpolationTest, DecomposeAndRecomposeIsIdentityForYShear) {
  Matrix3F input(Matrix3F::FromValues(1, 0, 0, 1, 1, 0, 0, 0, 1));

  EXPECT_TRUE(input.IsNear(DecomposeAndRecompose(input), 0.001f));
}

TEST(MatrixInterpolationTest, DecomposeAndRecomposeIsIdentityForRotation) {
  Matrix3F input(RotateMatrix(static_cast<float>(M_PI / 4)));

  EXPECT_TRUE(input.IsNear(DecomposeAndRecompose(input), 0.001f));
}

TEST(MatrixInterpolationTest, DecomposeAndRecomposeIsIdentityForScale) {
  Matrix3F input(ScaleMatrix(2.0f, 3.0f));

  EXPECT_TRUE(input.IsNear(DecomposeAndRecompose(input), 0.001f));
}

TEST(MatrixInterpolationTest, DecomposeAndRecomposeIsIdentityForTranslation) {
  Matrix3F input(TranslateMatrix(2.0f, 3.0f));

  EXPECT_TRUE(input.IsNear(DecomposeAndRecompose(input), 0.001f));
}

TEST(MatrixInterpolationTest,
     DecomposeAndRecomposeIsIdentityForShearAndTranslation) {
  // The W3C specification multiplies the translation values by the shear values
  // in matrix recomposition, but this is not how decomposition is done, and
  // thus an exact implementation of their algorithm will fail this test.
  // Failing this test (or any of these decompose and recompose tests) will
  // result in jarring animations when transitioning from within a transition.
  Matrix3F input(Matrix3F::FromValues(1, 1, 40, 0, 1, 40, 0, 0, 1));

  EXPECT_TRUE(input.IsNear(DecomposeAndRecompose(input), 0.001f));
}

TEST(MatrixInterpolationTest, InterpolatingIdentitiesGivesIdentity) {
  // We expect that interpolating two identity matrices will give us the
  // identity matrix back.
  Matrix3F a(Matrix3F::Identity());
  Matrix3F b(Matrix3F::Identity());

  static const int kNumSamples = 10;
  for (int i = 0; i < kNumSamples + 1; ++i) {
    math::Matrix3F interpolated =
        InterpolateMatrices(a, b, i / static_cast<float>(kNumSamples));
    EXPECT_EQ(Matrix3F::Identity(), interpolated);
  }
}

}  // namespace math
}  // namespace cobalt
