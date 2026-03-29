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

#include "starboard/nplb/time_zone_with_expect_value.h"

#include <gmock/gmock.h>
#include <array>

#include "starboard/extension/time_zone.h"
#include "starboard/nplb/time_constants.h"
#include "starboard/system.h"
#include "starboard/time_zone.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class SbTimeZoneGetCurrentSetTimeZoneTest
    : public testing::Test,
      public testing::WithParamInterface<TimeZoneWithExpectValue> {
 protected:
  void SetUp() override {
    time_zone_extension = static_cast<const StarboardExtensionTimeZoneApi*>(
        SbSystemGetExtension(kStarboardExtensionTimeZoneName));
    if (!time_zone_extension) {
      GTEST_SKIP()
          << "Skipping test for platform with missing Time Zone Extension.";
    }
    ASSERT_STREQ(time_zone_extension->name, kStarboardExtensionTimeZoneName);
    ASSERT_EQ(time_zone_extension->version, 1u);
    savedTimeZone = SbTimeZoneGetName();
  }

  void TearDown() override {
    if (time_zone_extension) {
      time_zone_extension->SetTimeZone(savedTimeZone.c_str());
    }
  }

  std::string savedTimeZone;
  const StarboardExtensionTimeZoneApi* time_zone_extension;
};

TEST(SbTimeZoneGetCurrentTest, IsKindOfSane) {
  SbTimeZone zone = SbTimeZoneGetCurrent();

  // This should always be a multiple of 15 minutes. Sadly, there are some
  // quarter-hour timezones, so our assertions can't be that strict.
  // https://en.wikipedia.org/wiki/Time_zone#Worldwide_time_zones
  EXPECT_EQ(zone % 15, 0);

  // The timezone should be between -24 hours...
  EXPECT_GE(zone, -24 * 60);

  // ... and +24 hours from the Prime Meridian, inclusive
  EXPECT_LE(zone, 24 * 60);
}

TEST_P(SbTimeZoneGetCurrentSetTimeZoneTest, IsKindOfSane) {
  EXPECT_TRUE(time_zone_extension->SetTimeZone(GetParam().timeZoneName.c_str()));
  auto zone = SbTimeZoneGetCurrent();
  EXPECT_THAT(zone, ::testing::AnyOf(GetParam().expectedStandardValue,
                                     GetParam().expectedDaylightValue));
}

INSTANTIATE_TEST_SUITE_P(SbTimeZoneGetCurrentSetTimeZoneTest,
                         SbTimeZoneGetCurrentSetTimeZoneTest,
                         ::testing::ValuesIn(timeZonesWithExpectedTimeValues));

}  // namespace
}  // namespace nplb
}  // namespace starboard
