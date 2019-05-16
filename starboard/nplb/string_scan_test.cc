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

// Here we are not trying to do anything fancy, just to really sanity check that
// this is hooked up to something.

#include "starboard/common/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const char kIpGood[] = "192.168.1.2";
const char kIpBad1[] = "192.1x8.1.2";
const char kIpBad2[] = "192.1x8";
const char kIpBad3[] = "x.1.2.3";
const char kIpBad4[] = "192.168.1.2.";
const char kIpFormat[] = "%u.%u.%u.%u";

const char kFloatString[] = "3.14159";
const char kFloatPattern[] = "%f";
const char kDoublePattern[] = "%lf";
const float kExpectedFloat = 3.14159f;
const double kExpectedDouble = 3.14159;

TEST(SbStringScanTest, SunnyDayIp1) {
  unsigned int in[4] = {0};
  EXPECT_EQ(4, SbStringScanF(kIpGood, kIpFormat, in, in + 1, in + 2, in + 3));
  EXPECT_EQ(192, in[0]);
  EXPECT_EQ(168, in[1]);
  EXPECT_EQ(1, in[2]);
  EXPECT_EQ(2, in[3]);
}

TEST(SbStringScanTest, SunnyDayIp2) {
  unsigned int in[4] = {0};
  EXPECT_EQ(4, SbStringScanF(kIpBad4, kIpFormat, in, in + 1, in + 2, in + 3));
  EXPECT_EQ(192, in[0]);
  EXPECT_EQ(168, in[1]);
  EXPECT_EQ(1, in[2]);
  EXPECT_EQ(2, in[3]);
}

TEST(SbStringScanTest, SunnyDayFloat) {
  float f = 0.0f;
  EXPECT_EQ(1, SbStringScanF(kFloatString, kFloatPattern, &f));
  EXPECT_EQ(kExpectedFloat, f);

  double d = 0.0;
  EXPECT_EQ(1, SbStringScanF(kFloatString, kDoublePattern, &d));
  EXPECT_EQ(kExpectedDouble, d);
}

TEST(SbStringScanTest, RainyDayIp1) {
  unsigned int in[4] = {0};
  EXPECT_EQ(2, SbStringScanF(kIpBad1, kIpFormat, in, in + 1, in + 2, in + 3));
  EXPECT_EQ(192, in[0]);
  EXPECT_EQ(1, in[1]);
  EXPECT_EQ(0, in[2]);
  EXPECT_EQ(0, in[3]);
}

TEST(SbStringScanTest, RainyDayIp2) {
  unsigned int in[4] = {0};
  EXPECT_EQ(2, SbStringScanF(kIpBad2, kIpFormat, in, in + 1, in + 2, in + 3));
  EXPECT_EQ(192, in[0]);
  EXPECT_EQ(1, in[1]);
  EXPECT_EQ(0, in[2]);
  EXPECT_EQ(0, in[3]);
}

TEST(SbStringScanTest, RainyDayIp3) {
  unsigned int in[4] = {0};
  EXPECT_EQ(0, SbStringScanF(kIpBad3, kIpFormat, in, in + 1, in + 2, in + 3));
  EXPECT_EQ(0, in[0]);
  EXPECT_EQ(0, in[1]);
  EXPECT_EQ(0, in[2]);
  EXPECT_EQ(0, in[3]);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
