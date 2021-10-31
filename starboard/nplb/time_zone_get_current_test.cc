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

#include "starboard/nplb/time_constants.h"
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

  if (zone == 0) {
    SB_LOG(WARNING) << "SbTimeZoneGetCurrent() returns 0. This is only correct "
                       "if the current time zone is the same as UTC";
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
