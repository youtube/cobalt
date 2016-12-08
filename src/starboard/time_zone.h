// Copyright 2015 Google Inc. All Rights Reserved.
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

// Access to the system time zone information.

#ifndef STARBOARD_TIME_ZONE_H_
#define STARBOARD_TIME_ZONE_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// The number of minutes west of the Greenwich Prime Meridian, NOT including any
// Daylight Savings Time adjustments.
//
// For example: PST/PDT is 480 minutes (28800 seconds, 8 hours).
typedef int SbTimeZone;

// Gets the system's current SbTimeZone.
SB_EXPORT SbTimeZone SbTimeZoneGetCurrent();

// Gets the three-letter code of the current timezone in standard time,
// regardless of current DST status. (e.g. "PST")
SB_EXPORT const char* SbTimeZoneGetName();

// Gets the three-letter code of the current timezone in Daylight Savings Time,
// regardless of current DST status. (e.g. "PDT")
SB_EXPORT const char* SbTimeZoneGetDstName();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_TIME_ZONE_H_
