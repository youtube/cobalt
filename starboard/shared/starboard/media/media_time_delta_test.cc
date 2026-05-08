// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/media_time_delta.h"

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(MediaTimeDeltaTest, ConstructionAndConversion) {
  MediaTimeDelta t1 = MediaTimeDelta::FromNanoseconds(1000);
  EXPECT_EQ(t1.InNanoseconds(), 1000);
  EXPECT_EQ(t1.InMicroseconds(), 1);

  MediaTimeDelta t2 = MediaTimeDelta::FromMicroseconds(1000);
  EXPECT_EQ(t2.InMicroseconds(), 1000);
  EXPECT_EQ(t2.InMilliseconds(), 1);

  MediaTimeDelta t3 = MediaTimeDelta::FromMilliseconds(1000);
  EXPECT_EQ(t3.InMilliseconds(), 1000);
  EXPECT_EQ(t3.InSeconds(), 1.0);

  MediaTimeDelta t4 = MediaTimeDelta::FromSeconds(1.5);
  EXPECT_EQ(t4.InSeconds(), 1.5);
  EXPECT_EQ(t4.InMilliseconds(), 1500);
}

TEST(MediaTimeDeltaTest, Arithmetic) {
  MediaTimeDelta t1 = MediaTimeDelta::FromMicroseconds(1000);
  MediaTimeDelta t2 = MediaTimeDelta::FromMicroseconds(500);

  MediaTimeDelta sum = t1 + t2;
  EXPECT_EQ(sum.InMicroseconds(), 1500);

  MediaTimeDelta diff = t1 - t2;
  EXPECT_EQ(diff.InMicroseconds(), 500);

  t1 += t2;
  EXPECT_EQ(t1.InMicroseconds(), 1500);

  t1 -= t2;
  EXPECT_EQ(t1.InMicroseconds(), 1000);
}

TEST(MediaTimeDeltaTest, Comparison) {
  MediaTimeDelta t1 = MediaTimeDelta::FromMicroseconds(1000);
  MediaTimeDelta t2 = MediaTimeDelta::FromMicroseconds(500);
  MediaTimeDelta t3 = MediaTimeDelta::FromMicroseconds(1000);

  EXPECT_TRUE(t1 > t2);
  EXPECT_TRUE(t2 < t1);
  EXPECT_TRUE(t1 == t3);
  EXPECT_TRUE(t1 != t2);
  EXPECT_TRUE(t1 >= t2);
  EXPECT_TRUE(t1 >= t3);
  EXPECT_TRUE(t2 <= t1);
}

TEST(MediaTimeDeltaTest, NegativeDurations) {
  MediaTimeDelta t1 = MediaTimeDelta::FromMicroseconds(-1000);
  EXPECT_EQ(t1.InMicroseconds(), -1000);
  EXPECT_LT(t1.InNanoseconds(), 0);

  MediaTimeDelta t2 = MediaTimeDelta::FromMicroseconds(500);
  MediaTimeDelta sum = t1 + t2;
  EXPECT_EQ(sum.InMicroseconds(), -500);

  EXPECT_TRUE(t1 < t2);
}

TEST(MediaTimeDeltaTest, ExtremeValues) {
  MediaTimeDelta t_max =
      MediaTimeDelta::FromNanoseconds(std::numeric_limits<int64_t>::max());
  EXPECT_EQ(t_max.InNanoseconds(), std::numeric_limits<int64_t>::max());

  MediaTimeDelta t_min =
      MediaTimeDelta::FromNanoseconds(std::numeric_limits<int64_t>::min());
  EXPECT_EQ(t_min.InNanoseconds(), std::numeric_limits<int64_t>::min());
}

TEST(MediaTimeDeltaTest, PrecisionLoss) {
  MediaTimeDelta t1 = MediaTimeDelta::FromSeconds(1.5);
  EXPECT_EQ(t1.InNanoseconds(), 1500000000);

  MediaTimeDelta t2 = MediaTimeDelta::FromSeconds(1.1);
  EXPECT_NEAR(t2.InSeconds(), 1.1, 0.000001);
}

}  // namespace
}  // namespace starboard
