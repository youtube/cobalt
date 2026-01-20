// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_TIME_ZONE_WITH_EXPECT_VALUE_H_
#define STARBOARD_NPLB_TIME_ZONE_WITH_EXPECT_VALUE_H_

#include "starboard/time_zone.h"

#include <gmock/gmock.h>
#include <array>
#include <string>

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

extern const std::array<TimeZoneWithExpectValue, 12> timeZonesWithExpectedTimeValues;

#endif  // STARBOARD_NPLB_TIME_ZONE_WITH_EXPECT_VALUE_H_
