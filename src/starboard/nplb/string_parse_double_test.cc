// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// Here we are not trying to do anything comprehensive, just to sanity check
// that this is hooked up to something.

#include <float.h>  // for DBL_MIN
#include <math.h>   // for HUGE_VAL
#include "starboard/common/string.h"
#include "starboard/double.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION < 13

namespace starboard {
namespace nplb {
namespace {

// positive number
TEST(SbStringParseDoubleTest, Positive) {
  const char kDouble[] = "  123.4";
  const double kExpected = 123.4;
  char* end = NULL;
  EXPECT_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 7, end);
}

// positive number without decimal
TEST(SbStringParseDoubleTest, PositiveNoDecimal) {
  const char kDouble[] = "  123";
  const double kExpected = 123;
  char* end = NULL;
  EXPECT_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 5, end);
}

// negative number
TEST(SbStringParseDoubleTest, Negative) {
  const char kDouble[] = "  -123.4";
  const double kExpected = -123.4;
  char* end = NULL;
  EXPECT_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 8, end);
}

// zero (in case it gets confused with null)
TEST(SbStringParseDoubleTest, Zero) {
  const char kDouble[] = "0";
  const double kExpected = 0;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 1, end);
}

TEST(SbStringParseDoubleTest, TabSpacesNewLinePrefix) {
  const char kDouble[] = "\t \n123.5";
  const double kExpected = 123.5;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 8, end);
}

TEST(SbStringParseDoubleTest, TabSpacesNewLineSuffix) {
  const char kDouble[] = "123.5\t \n";
  const double kExpected = 123.5;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 5, end);
}

TEST(SbStringParseDoubleTest, ScientificLowerCasePositiveExp) {
  const char kDouble[] = "123.5e2";
  const double kExpected = 12350;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 7, end);
}

TEST(SbStringParseDoubleTest, ScientificLowerCasePositiveExp2) {
  const char kDouble[] = "123.5e+2";
  const double kExpected = 12350;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 8, end);
}

TEST(SbStringParseDoubleTest, ScientificLowerCaseNegativeExp) {
  const char kDouble[] = "123.5e-2";
  const double kExpected = 1.235;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 8, end);
}

TEST(SbStringParseDoubleTest, ScientificUpperCasePositiveExp) {
  const char kDouble[] = "123.5E2";
  const double kExpected = 12350;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 7, end);
}

TEST(SbStringParseDoubleTest, ScientificUpperCasePositiveExp2) {
  const char kDouble[] = "123.5E+2";
  const double kExpected = 12350;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 8, end);
}

TEST(SbStringParseDoubleTest, ScientificUpperCaseNegativeExp) {
  const char kDouble[] = "123.5E-2";
  const double kExpected = 1.235;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 8, end);
}

TEST(SbStringParseDoubleTest, Empty) {
  const char kDouble[] = "";
  const double kExpected = 0.0;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 0, end);
}

TEST(SbStringParseDoubleTest, Space) {
  const char kDouble[] = " ";
  const double kExpected = 0.0;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 0, end);
}

TEST(SbStringParseDoubleTest, AlphaNumerics) {
  const char kDouble[] = "alskjdfkldd2";
  const double kExpected = 0.0;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 0, end);
}

// out of bounds
TEST(SbStringParseDoubleTest, Huge) {
  const char kDouble[] = "9e9999";
  const double kExpected = HUGE_VAL;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 6, end);
}

TEST(SbStringParseDoubleTest, HugeNeg) {
  const char kDouble[] = "-9e9999";
  const double kExpected = -HUGE_VAL;
  char* end = NULL;
  EXPECT_DOUBLE_EQ(kExpected, SbStringParseDouble(kDouble, &end));
  EXPECT_EQ(kDouble + 7, end);
}

// Some platforms do not support denormals
// Denormal support is unnecessary for Cobalt as of Sep 7, 2016
TEST(SbStringParseDoubleTest, DISABLED_Small) {
  const char kDouble[] = "1e-310";
  const double kExpected = DBL_MIN;
  char* end = NULL;
  double answer(SbStringParseDouble(kDouble, &end));
  EXPECT_GE(kExpected, answer);
  EXPECT_GT(answer, 0.0);
  EXPECT_EQ(kDouble + 6, end);
}

// Some platforms do not support denormals
// Denormal support is unnecessary for Cobalt as of Sep 7, 2016
TEST(SbStringParseDoubleTest, DISABLED_SmallNeg) {
  const char kDouble[] = "-1e-310";
  const double kExpected = -DBL_MIN;
  char* end = NULL;
  double answer(SbStringParseDouble(kDouble, &end));
  EXPECT_LE(kExpected, answer);
  EXPECT_LT(answer, 0.0);
  EXPECT_EQ(kDouble + 7, end);
}

// The following tests are disabled until we decide we want to mimic
// C11 behavior in Starboard.  NAN and INF supported was added in C11
// test NAN, and INFINITY
TEST(SbStringParseDoubleTest, NaN) {
  const char kNanDoubles[][4] = {"nan", "naN", "nAn", "nAN",
                                 "Nan", "NaN", "NAn", "NAN"};
  const double kExpected = NAN;
  char* end = NULL;
  for (size_t i(0); i != sizeof(kNanDoubles) / sizeof(kNanDoubles[0]); ++i) {
    EXPECT_TRUE(SbDoubleIsNan(SbStringParseDouble(kNanDoubles[i], &end)));
    EXPECT_EQ(kNanDoubles[i] + 3, end);
  }
}

TEST(SbStringParseDoubleTest, PosInf) {
  const char kInfDoubles[][4] = {"inf", "inF", "iNf", "iNF",
                                 "Inf", "InF", "INf", "INF"};
  char* end = NULL;
  for (size_t i(0); i != sizeof(kInfDoubles) / sizeof(kInfDoubles[0]); ++i) {
    EXPECT_FALSE(SbDoubleIsFinite(SbStringParseDouble(kInfDoubles[i], &end)));
    EXPECT_EQ(kInfDoubles[i] + 3, end);
  }

  const char kInfinity[] = "InFinIty";
  end = NULL;
  EXPECT_FALSE(SbDoubleIsFinite(SbStringParseDouble(kInfinity, &end)));
  EXPECT_EQ(kInfinity + 8, end);
}

TEST(SbStringParseDoubleTest, PosInf2) {
  const char kInfDoubles[][5] = {"+inf", "+inF", "+iNf", "+iNF",
                                 "+Inf", "+InF", "+INf", "+INF"};
  char* end = NULL;
  for (size_t i(0); i != sizeof(kInfDoubles) / sizeof(kInfDoubles[0]); ++i) {
    EXPECT_FALSE(SbDoubleIsFinite(SbStringParseDouble(kInfDoubles[i], &end)));
    EXPECT_EQ(kInfDoubles[i] + 4, end);
  }

  const char kInfinity[] = "+InFinIty";
  end = NULL;
  EXPECT_FALSE(SbDoubleIsFinite(SbStringParseDouble(kInfinity, &end)));
  EXPECT_EQ(kInfinity + 9, end);
}

TEST(SbStringParseDoubleTest, NegInf) {
  const char kInfDoubles[][5] = {"-inf", "-inF", "-iNf", "-iNF",
                                 "-Inf", "-InF", "-INf", "-INF"};
  char* end = NULL;
  for (size_t i(0); i != sizeof(kInfDoubles) / sizeof(kInfDoubles[0]); ++i) {
    EXPECT_FALSE(SbDoubleIsFinite(SbStringParseDouble(kInfDoubles[i], &end)));
    EXPECT_EQ(kInfDoubles[i] + 4, end);
  }

  const char kInfinity[] = "-InFinIty";
  end = NULL;
  EXPECT_FALSE(SbDoubleIsFinite(SbStringParseDouble(kInfinity, &end)));
  EXPECT_EQ(kInfinity + 9, end);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION < 13
