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
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/time_zone.h"

#include <string>
#include <Windows.h>

namespace starboard {
namespace xb1 {
namespace shared {

namespace {

bool SetTimeZone(const char* time_zone_name) {
  Windows::Globalization::Calendar ^ calendar = ref new Windows::Globalization::Calendar();
  Platform::String ^ value_ = starboard::shared::win32::stringToPlatformString(time_zone_name);
  calendar->ChangeTimeZone(value_);
  return SbTimeZoneGetName() == std::string(time_zone_name);
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

}  // namespace shared
}  // namespace xb1
}  // namespace starboard
