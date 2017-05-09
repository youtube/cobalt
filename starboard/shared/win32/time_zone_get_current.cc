// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/time_zone.h"

#include <windows.h>

#include "starboard/log.h"
SbTimeZone SbTimeZoneGetCurrent() {
  DYNAMIC_TIME_ZONE_INFORMATION time_zone_info;

  DWORD zone_id = GetDynamicTimeZoneInformation(&time_zone_info);

  switch (zone_id) {
  case TIME_ZONE_ID_UNKNOWN:
    return time_zone_info.Bias;
  case TIME_ZONE_ID_STANDARD:
    return time_zone_info.StandardBias;
  case TIME_ZONE_ID_DAYLIGHT:
    return time_zone_info.DaylightBias;
  default:
    SB_NOTREACHED();
    return 0;
  }
}
