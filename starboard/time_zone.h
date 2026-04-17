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

// Module Overview: Starboard Time Zone module
//
// Provides access to the system time zone information.

#ifndef STARBOARD_TIME_ZONE_H_
#define STARBOARD_TIME_ZONE_H_

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// The number of minutes west of the Greenwich Prime Meridian, NOT including
// Daylight Savings Time adjustments.
//
// For example: America/Los_Angeles is 480 minutes (28800 seconds, 8 hours).
typedef int SbTimeZone;

// Gets a string representation of the current timezone. The format should be
// in the IANA format https://data.iana.org/time-zones/theory.html#naming .
// Names normally have the form AREA/LOCATION, where AREA is a continent or
// ocean, and LOCATION is a specific location within the area.
// Typical names are 'Africa/Cairo', 'America/New_York', and 'Pacific/Honolulu'.
SB_EXPORT const char* SbTimeZoneGetName();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_TIME_ZONE_H_
