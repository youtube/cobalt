// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <time.h>

#if SB_API_VERSION < SB_TIME_ZONE_FLEXIBLE_API_VERSION
const char* SbTimeZoneGetDstName() {
  // Note tzset() is called in ApplicationAndroid::Initialize()

  // Android's bionic seems not to set tzname[1] when selecting GMT
  // because timezone is not otherwise available.
  // But glibc returns "GMT" in both tzname[0] and tzname[1] when
  // GMT is selected.
  if (tzname[1][0] == '\0') {
    return tzname[0];
  } else {
    return tzname[1];
  }
}
#endif  // SB_API_VERSION < SB_TIME_ZONE_FLEXIBLE_API_VERSION
