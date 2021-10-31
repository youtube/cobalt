/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/math/linear_interpolator.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace math {

TEST(LinearInterpolator, SimpleInterpolation) {
  LinearInterpolator<float, float> interp;
  interp.Add(0, 0);
  interp.Add(1, 2);

  // Expect that value of 0.0f maps to 0.0f
  EXPECT_FLOAT_EQ(0.f, interp.Map(0.0f));
  // Expect that value of 0.5f maps to 1.0f
  EXPECT_FLOAT_EQ(1.f, interp.Map(0.5f));
  // Expect that value of 1.5f maps to 2.0f
  EXPECT_FLOAT_EQ(2.f, interp.Map(1.0f));
}

// Tests the expectation that clearing the interpolator works.
TEST(LinearInterpolator, Clear) {
  LinearInterpolator<int, int> interp;
  interp.Add(-1, 0);
  interp.Add(1, 2);

  interp.Clear();

  EXPECT_FLOAT_EQ(0, interp.Map(-1));
  EXPECT_FLOAT_EQ(0, interp.Map(1));
}

// Tests the expectation that interpolating on one value in the table works
// as expected: to always return that value.
TEST(LinearInterpolator, InterpolateSingularValue) {
  LinearInterpolator<size_t, size_t> interp;
  interp.Add(2, 10);

  EXPECT_EQ(10, interp.Map(2));
  EXPECT_EQ(10, interp.Map(1));
  EXPECT_EQ(10, interp.Map(3));
}

// Tests the expectation that we can introduce an discontinuity by passing
// in duplicate keys.
TEST(LinearInterpolator, Discontinuity) {
  LinearInterpolator<double, float> interp;
  interp.Add(0, 0);
  interp.Add(1, 1);  // Discontinuity at input = 1.
  interp.Add(1, 3);
  interp.Add(2, 4);

  static const double kErrorThreshold = .1f;
  static const double kEpsilon = std::numeric_limits<double>::epsilon() * 4.0;

  EXPECT_NEAR(1.0, interp.Map(1.f - kEpsilon), kErrorThreshold);
  // Expect that at the discontinuity point value of 1, that the value that
  // the interpolator produces is 3.
  EXPECT_FLOAT_EQ(3, interp.Map(1.f));
  EXPECT_NEAR(3.0, interp.Map(1.f + kEpsilon), kErrorThreshold);
}

// Tests that extrapolation is forbidden with this interpolator.
TEST(LinearInterpolator, ExtrapolationForbidden) {
  LinearInterpolator<float, float> interp;
  interp.Add(0, 0);
  interp.Add(1, 2);

  // Expect that values less than the minimal key value is clamped.
  EXPECT_FLOAT_EQ(0.0f, interp.Map(-1.0f));
  // Expect that values greater than the maximum key value is clamped.
  EXPECT_FLOAT_EQ(2.0f, interp.Map(2.0f));
}

// Some more real world values.
TEST(LinearInterpolator, UseComplexFloat) {
  LinearInterpolator<float, float> interp;
  interp.Add(0, 0);
  interp.Add(41, 1);
  interp.Add(1023, 15);

  EXPECT_FLOAT_EQ(0.0f, interp.Map(0.f));
  EXPECT_FLOAT_EQ(0.5f, interp.Map(41.f / 2.f));
  EXPECT_FLOAT_EQ(1.0f, interp.Map(41.f));
  EXPECT_FLOAT_EQ(15.f, interp.Map(1023.f));
}

// Tests that this interpolator works with integer keys and float values.
TEST(LinearInterpolator, UseIntegerKeyWithFloatValue) {
  LinearInterpolator<int, float> interp;
  interp.Add(0, 0.f);
  interp.Add(10, 1.f);
  interp.Add(100, 2.f);

  EXPECT_FLOAT_EQ(0.0f, interp.Map(0));
  EXPECT_FLOAT_EQ(0.5f, interp.Map(5));
  EXPECT_FLOAT_EQ(1.0f, interp.Map(10));
  EXPECT_FLOAT_EQ(2.0f, interp.Map(100));
}

// Tests that this interpolator works with integer keys and int values.
TEST(LinearInterpolator, UseIntegerKeyWithIntegerValue) {
  LinearInterpolator<int, int> interp;
  interp.Add(0, 0);
  interp.Add(10, 100);
  interp.Add(100, 1000);

  EXPECT_EQ(0, interp.Map(0));
  EXPECT_EQ(50, interp.Map(5));
  EXPECT_EQ(100, interp.Map(10));
  EXPECT_EQ(550, interp.Map(55));
  EXPECT_EQ(1000, interp.Map(100));
}

}  // namespace math
}  // namespace cobalt
