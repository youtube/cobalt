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
#include "starboard/system.h"
#include "starboard/time_zone.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

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

  static auto const* time_zone_extension =
      static_cast<const StarboardExtensionTimeZoneApi*>(
          SbSystemGetExtension(kStarboardExtensionTimeZoneName));
  if (time_zone_extension) {
    ASSERT_STREQ(time_zone_extension->name, kStarboardExtensionTimeZoneName);
    ASSERT_EQ(time_zone_extension->version, 1u);
    time_zone_extension->SetTimeZone("UTC");
    zone = SbTimeZoneGetCurrent();
    EXPECT_EQ(zone, 0);

    // Atlantic time zone, UTC−04:00
    time_zone_extension->SetTimeZone("America/Puerto_Rico");
    zone = SbTimeZoneGetCurrent();
    EXPECT_EQ(zone, 240);

    // Eastern time zone, UTC−05:00
    time_zone_extension->SetTimeZone("America/New_York");
    zone = SbTimeZoneGetCurrent();
    EXPECT_EQ(zone, 300);

    time_zone_extension->SetTimeZone("US/Eastern");
    zone = SbTimeZoneGetCurrent();
    EXPECT_EQ(zone, 300);

    // Central time zone, UTC−06:00
    time_zone_extension->SetTimeZone("America/Chicago");
    zone = SbTimeZoneGetCurrent();
    EXPECT_EQ(zone, 360);

    // Mountain time zone, UTC−07:00
    time_zone_extension->SetTimeZone("US/Mountain");
    zone = SbTimeZoneGetCurrent();
    EXPECT_EQ(zone, 420);

    // Pacific time zone, UTC-08:00
    time_zone_extension->SetTimeZone("US/Pacific");
    zone = SbTimeZoneGetCurrent();
    EXPECT_EQ(zone, 480);

    // Alaska time zone, UTC-09:00
    time_zone_extension->SetTimeZone("US/Alaska");
    zone = SbTimeZoneGetCurrent();
    EXPECT_EQ(zone, 540);

    // Hawaii-Aleutian time zone, UTC-10:00
    time_zone_extension->SetTimeZone("Pacific/Honolulu");
    zone = SbTimeZoneGetCurrent();
    EXPECT_EQ(zone, 600);

    // American Samoa time zone, UTC-11:00
    time_zone_extension->SetTimeZone("US/Samoa");
    zone = SbTimeZoneGetCurrent();
    EXPECT_EQ(zone, 660);

    // American Samoa time zone, UTC+10:00
    time_zone_extension->SetTimeZone("Pacific/Guam");
    zone = SbTimeZoneGetCurrent();
    EXPECT_EQ(zone, -600);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
