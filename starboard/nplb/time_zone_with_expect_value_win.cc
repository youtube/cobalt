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

#if defined(_WIN32)

const std::array<TimeZoneWithExpectValue, 12> timeZonesWithExpectedTimeValues{
    TimeZoneWithExpectValue("UTC", 0, 0),
    TimeZoneWithExpectValue("Atlantic Standard Time", 240, 180),
    TimeZoneWithExpectValue("Eastern Standard Time", 300, 240),
    TimeZoneWithExpectValue("Central Standard Time", 360, 300),
    TimeZoneWithExpectValue("Mountain Standard Time", 420, 360),
    TimeZoneWithExpectValue("Pacific Standard Time", 480, 420),
    TimeZoneWithExpectValue("Yukon Standard Time", 420, 420),
    TimeZoneWithExpectValue("Samoa Standard Time", -840, -840),
    TimeZoneWithExpectValue("China Standard Time", -480, -480),
    TimeZoneWithExpectValue("Central European Standard Time", -60, -120),
    TimeZoneWithExpectValue("Omsk Standard Time", -360, -360),
    TimeZoneWithExpectValue("Cen. Australia Standard Time", -570, -630)};

#endif
