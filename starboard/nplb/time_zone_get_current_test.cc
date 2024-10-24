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

#include "starboard/extension/time_zone.h"
#include "starboard/nplb/time_constants.h"
#include "starboard/system.h"
#include "starboard/time_zone.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <array>
#include <gmock/gmock.h>

namespace starboard {
namespace nplb {
namespace {

struct TimeZoneWithExpectValue {
  TimeZoneWithExpectValue(std::string timeZoneName_,
                          SbTimeZone expectedStandardValue_,
                          SbTimeZone expectedDaylightValue_)
      : timeZoneName{timeZoneName_},
        expectedStandardValue{expectedStandardValue_},
        expectedDaylightValue{expectedDaylightValue_} {}

  std::string timeZoneName;

  SbTimeZone expectedStandardValue;
  SbTimeZone expectedDaylightValue;
};

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

#if defined(_WIN32)

std::array<TimeZoneWithExpectValue, 12> timeZonesWithExpectedTimeValues{
    TimeZoneWithExpectValue("UTC", 0, 0),
    TimeZoneWithExpectValue("Atlantic Standard Time", 240, 180),
    TimeZoneWithExpectValue("Eastern Standard Time", 300, 240),
    TimeZoneWithExpectValue("Central Standard Time", 360, 300),
    TimeZoneWithExpectValue("Mountain Standard Time", 420, 360),
    TimeZoneWithExpectValue("Pacific Standard Time", 480, 420),
    TimeZoneWithExpectValue("Yukon Standard Time", 420, 420),
    TimeZoneWithExpectValue("Samoa Standard Time", -780, -780),
    TimeZoneWithExpectValue("China Standard Time", -480, -480),
    TimeZoneWithExpectValue("Central European Standard Time", -60, -120),
    TimeZoneWithExpectValue("Omsk Standard Time", -360, -360),
    TimeZoneWithExpectValue("Cen. Australia Standard Time", -570, -630)};

#else

std::array<TimeZoneWithExpectValue, 11> timeZonesWithExpectedTimeValues{
    TimeZoneWithExpectValue("UTC", 0, 0),
    TimeZoneWithExpectValue("America/Puerto_Rico", 240, 240),
    TimeZoneWithExpectValue("America/New_York", 300, 300),
    TimeZoneWithExpectValue("US/Eastern", 300, 300),
    TimeZoneWithExpectValue("America/Chicago", 360, 360),
    TimeZoneWithExpectValue("US/Mountain", 420, 420),
    TimeZoneWithExpectValue("US/Pacific", 480, 480),
    TimeZoneWithExpectValue("US/Alaska", 540, 540),
    TimeZoneWithExpectValue("Pacific/Honolulu", 600, 600),
    TimeZoneWithExpectValue("US/Samoa", 660, 660),
    TimeZoneWithExpectValue("Pacific/Guam", -600, -600)};

#endif

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
