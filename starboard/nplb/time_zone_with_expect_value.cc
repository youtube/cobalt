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

#include "starboard/nplb/time_zone_with_expect_value.h"

#if !defined(_WIN32)

const std::array<TimeZoneWithExpectValue, 12> timeZonesWithExpectedTimeValues{
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
    TimeZoneWithExpectValue("Australia/South", -570, -570),
    TimeZoneWithExpectValue("Pacific/Guam", -600, -600)};

#endif
