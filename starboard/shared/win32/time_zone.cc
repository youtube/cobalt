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

#include "starboard/extension/time_zone.h"
#include "starboard/time_zone.h"
#include "starboard/common/log.h"

#include <string>
#include <Windows.h>

namespace starboard {
namespace shared {
namespace win32 {

namespace {

bool SetTimeZone(const char* time_zone_name) {
  std::string tzName(time_zone_name);
  std::wstring windowsTzName(tzName.begin(), tzName.end());

  DYNAMIC_TIME_ZONE_INFORMATION dtzi{0};
  TIME_ZONE_INFORMATION tzi{0};
  int index = 0;
  bool found = false;

  while (EnumDynamicTimeZoneInformation(index, &dtzi) == ERROR_SUCCESS) {
    if (windowsTzName == dtzi.TimeZoneKeyName ||
        windowsTzName == dtzi.StandardName ||
        windowsTzName == dtzi.DaylightName) {
      found = true;
      break;
    }
    index++;
  }
  if (!found) {
    SB_LOG(ERROR) << "Time zone: " << tzName.c_str() << "not found.";
    return false;
  }
  auto result = SetDynamicTimeZoneInformation(&dtzi);
  if (result == 0) {
    DWORD error = GetLastError();
    SB_LOG(ERROR) << "SetDynamicTimeZoneInformation failed for  time zone: "
              << tzName.c_str() << " return code: " << result
              << " last error: " << error;
    return false;
  }

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
