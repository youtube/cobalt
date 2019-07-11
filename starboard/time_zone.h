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
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// The number of minutes west of the Greenwich Prime Meridian, NOT including
// Daylight Savings Time adjustments.
//
// For example: PST/PDT is 480 minutes (28800 seconds, 8 hours).
typedef int SbTimeZone;

// Gets the system's current SbTimeZone in minutes.
SB_EXPORT SbTimeZone SbTimeZoneGetCurrent();

// Gets a string representation of the current timezone. Note that the string
// representation can either be standard or daylight saving time. The output
// can be of the form:
//   1) A three-letter abbreviation such as "PST" or "PDT" (preferred).
//   2) A time zone identifier such as "America/Los_Angeles"
//   3) An un-abbreviated name such as "Pacific Standard Time".
SB_EXPORT const char* SbTimeZoneGetName();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_TIME_ZONE_H_
