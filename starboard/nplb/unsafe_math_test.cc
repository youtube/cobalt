// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#include <limits>

#include "starboard/double.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION < 13

namespace starboard {
namespace nplb {
namespace {

TEST(SbUnsafeMathTest, NaNDoubleSunnyDay) {
  auto a = std::numeric_limits<double>::quiet_NaN();
  auto b = std::numeric_limits<double>::quiet_NaN();
  auto c = 42.5;
  auto infinity = std::numeric_limits<double>::infinity();

  // A NaN is a NaN.
  EXPECT_TRUE(SbDoubleIsNan(a));
  EXPECT_TRUE(SbDoubleIsNan(b));

  // However, NaN is not ordered.  It is not greater than, less than, or
  // equal to anything, including itself.
  EXPECT_FALSE(a == a);
  EXPECT_FALSE(a > a);
  EXPECT_FALSE(a < a);
  EXPECT_FALSE(a >= a);
  EXPECT_FALSE(a <= a);

  EXPECT_FALSE(a == b);
  EXPECT_FALSE(a > b);
  EXPECT_FALSE(a < b);
  EXPECT_FALSE(a >= b);
  EXPECT_FALSE(a <= b);

  EXPECT_FALSE(a == c);
  EXPECT_FALSE(a > c);
  EXPECT_FALSE(a < c);
  EXPECT_FALSE(a >= c);
  EXPECT_FALSE(a <= c);

  EXPECT_FALSE(c == a);
  EXPECT_FALSE(c > a);
  EXPECT_FALSE(c < a);
  EXPECT_FALSE(c >= a);
  EXPECT_FALSE(c <= a);

  // All operations involving a NaN result in a NaN.
  EXPECT_TRUE(SbDoubleIsNan(a + b));
  EXPECT_TRUE(SbDoubleIsNan(a * b));
  EXPECT_TRUE(SbDoubleIsNan(a - b));
  EXPECT_TRUE(SbDoubleIsNan(a / b));

  EXPECT_TRUE(SbDoubleIsNan(a + c));
  EXPECT_TRUE(SbDoubleIsNan(a * c));
  EXPECT_TRUE(SbDoubleIsNan(a - c));
  EXPECT_TRUE(SbDoubleIsNan(a / c));

  EXPECT_TRUE(SbDoubleIsNan(c + a));
  EXPECT_TRUE(SbDoubleIsNan(c * a));
  EXPECT_TRUE(SbDoubleIsNan(c - a));
  EXPECT_TRUE(SbDoubleIsNan(c / a));

  // (+/- infinity / +/- infinity) results in a NaN.
  EXPECT_TRUE(SbDoubleIsNan(infinity / infinity));
  EXPECT_TRUE(SbDoubleIsNan(-infinity / infinity));
  EXPECT_TRUE(SbDoubleIsNan(infinity / -infinity));
  EXPECT_TRUE(SbDoubleIsNan(-infinity / -infinity));

  // Infinity minus infinity (and signed equivalents) result in a NaN.
  EXPECT_TRUE(SbDoubleIsNan(infinity + -infinity));
  EXPECT_TRUE(SbDoubleIsNan(-infinity + infinity));
  EXPECT_TRUE(SbDoubleIsNan(infinity - infinity));
  EXPECT_TRUE(SbDoubleIsNan(-infinity - -infinity));

  EXPECT_TRUE(SbDoubleIsNan(std::sqrt(-c)));
}

TEST(SbUnsafeMathTest, InfinityDoubleSunnyDay) {
  auto a = std::numeric_limits<double>::infinity();
  auto b = std::numeric_limits<double>::infinity();

  // Infinity is equal to itself.
  EXPECT_TRUE(a == b);
  EXPECT_TRUE(-a == -b);

  // Infinity is greater than the maximum double.
  EXPECT_TRUE(a > std::numeric_limits<double>::max());
  EXPECT_TRUE(a >= std::numeric_limits<double>::max());

  // Negative infinity is less than the minimum double.
  EXPECT_TRUE(-a < std::numeric_limits<double>::min());
  EXPECT_TRUE(-a <= std::numeric_limits<double>::min());
}

TEST(SbUnsafeMathTest, SignedZeroDoubleSunnyDay) {
  auto a = 42.5;
  auto positive_zero = 0.0;
  auto negative_zero = -0.0;
  auto infinity = std::numeric_limits<double>::infinity();

  EXPECT_TRUE(positive_zero == negative_zero);

  EXPECT_TRUE(negative_zero / a == -positive_zero);

  EXPECT_TRUE(negative_zero * negative_zero == positive_zero);

  EXPECT_TRUE(a + positive_zero == a);
  EXPECT_TRUE(a + negative_zero == a);

  EXPECT_TRUE(negative_zero + negative_zero == negative_zero - positive_zero);
  EXPECT_TRUE(negative_zero - positive_zero == negative_zero);

  EXPECT_TRUE(positive_zero + positive_zero == positive_zero - negative_zero);
  EXPECT_TRUE(positive_zero - negative_zero == positive_zero);

  EXPECT_TRUE(a - a == a + (-a));
  EXPECT_TRUE(a + (-a) == positive_zero);

  EXPECT_TRUE(std::sqrt(negative_zero) == negative_zero);
  EXPECT_TRUE(negative_zero / -infinity == positive_zero);

  EXPECT_TRUE(SbDoubleAbsolute(a) / negative_zero == -infinity);

  EXPECT_TRUE(SbDoubleIsNan(positive_zero * infinity));
  EXPECT_TRUE(SbDoubleIsNan(-positive_zero * infinity));
  EXPECT_TRUE(SbDoubleIsNan(positive_zero * -infinity));
  EXPECT_TRUE(SbDoubleIsNan(-positive_zero * -infinity));

  EXPECT_TRUE(SbDoubleIsNan(positive_zero / positive_zero));
  EXPECT_TRUE(SbDoubleIsNan(-positive_zero / positive_zero));
  EXPECT_TRUE(SbDoubleIsNan(positive_zero / -positive_zero));
  EXPECT_TRUE(SbDoubleIsNan(-positive_zero / -positive_zero));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 13
