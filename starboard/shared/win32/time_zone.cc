// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/win32/time_zone.h"

#include "starboard/extension/time_zone.h"

#include <stdlib.h>
#include <time.h>
#include <cstring>

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace win32 {

namespace {

// Definitions of any functions included as components in the extension
// are added here.

bool SetTimeZone(const char* time_zone_name) {
  SB_LOG(WARNING) << "Start to set time zone in windows now!!!"
                  << time_zone_name;
  if (time_zone_name == nullptr || strlen(time_zone_name) == 0) {
    SB_LOG(ERROR) << "Set time zone failed!";
    SB_LOG(ERROR) << "Time zone name can't be null or empty string.";
    return false;
  }
  if (_putenv_s("TZ", time_zone_name) != 0) {
    SB_LOG(WARNING) << "Set time zone failed!";
    return false;
  }
  char* tz = getenv("TZ");
  SB_LOG(WARNING) << "get env variable" << tz;
  _tzset();
  size_t s;
  char tzname[100];
  _get_tzname(&s, tzname, sizeof(tzname), 0);
  long timezone;
  _get_timezone( &timezone );
  SB_LOG(WARNING) << "timezone: " << timezone;
  SB_LOG(WARNING) << "Successfully set timezone in windows!!" << tzname;
  return true;
}

const StarboardExtensionTimeZoneApi kTimeZoneApi = {
    kStarboardExtensionTimeZoneName,
    1,  // API version that's implemented.
    &SetTimeZone,
};

}  // namespace

const void* GetTimeZoneApi() {
  return &kTimeZoneApi;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
