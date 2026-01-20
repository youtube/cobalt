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

#include "starboard/common/log.h"
#include "starboard/extension/time_zone.h"
#include "starboard/time_zone.h"

#include <Windows.h>
#include <string>

namespace starboard {
namespace shared {
namespace win32 {

namespace {

bool SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, bool bEnablePrivilege) {
  TOKEN_PRIVILEGES tp;
  LUID luid;

  if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid)) {
    SB_LOG(ERROR) << "LookupPrivilegeValue error: " << GetLastError();
    return false;
  }

  tp.PrivilegeCount = 1;
  tp.Privileges[0].Luid = luid;
  tp.Privileges[0].Attributes = bEnablePrivilege ? SE_PRIVILEGE_ENABLED : 0;

  if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES),
                             (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL)) {
    SB_LOG(ERROR) << "AdjustTokenPrivileges error: " << GetLastError();
    return false;
  }

  if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
    SB_LOG(ERROR) << "The token does not have the specified privilege.";
    return false;
  }

  return true;
}

bool SetTimeZone(const char* time_zone_name) {
  std::string tzName(time_zone_name);
  std::wstring windowsTzName(tzName.begin(), tzName.end());

  HANDLE hToken;
  if (!OpenProcessToken(GetCurrentProcess(),
                        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
    SB_LOG(ERROR) << "OpenProcessToken error: " << GetLastError();
    return false;
  }

  if (!SetPrivilege(hToken, SE_TIME_ZONE_NAME, true)) {
    return false;
  }

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

  if (!SetPrivilege(hToken, SE_TIME_ZONE_NAME, false)) {
    return false;
  }

  CloseHandle(hToken);

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
