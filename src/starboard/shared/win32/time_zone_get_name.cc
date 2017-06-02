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

#include <string>
#include <Windows.h>

#include "starboard/once.h"
#include "starboard/shared/win32/wchar_utils.h"

namespace {
class TimeZoneString {
 public:
  static TimeZoneString* Get();
  const char* value() const { return value_.c_str(); }

 private:
  TimeZoneString() {
    DYNAMIC_TIME_ZONE_INFORMATION time_zone_info;
    DWORD zone_id = GetDynamicTimeZoneInformation(&time_zone_info);

    std::wstring wide_string = time_zone_info.TimeZoneKeyName;
    value_ = starboard::shared::win32::wchar_tToUTF8(wide_string.c_str());
  }
  std::string value_;
};

SB_ONCE_INITIALIZE_FUNCTION(TimeZoneString, TimeZoneString::Get);
}  // namespace.

const char* SbTimeZoneGetName() {
  const char* output = TimeZoneString::Get()->value();
  return output;
}
