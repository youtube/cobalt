// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/system.h"

#include <string>

#include "starboard/common/log.h"
#include "starboard/shared/win32/wchar_utils.h"

using Windows::System::Profile::AnalyticsInfo;
using Windows::System::Profile::AnalyticsVersionInfo;

#if SB_API_VERSION < 15

SbSystemDeviceType SbSystemGetDeviceType() {
  AnalyticsVersionInfo ^ version_info = AnalyticsInfo::VersionInfo;
  std::string family = starboard::shared::win32::platformStringToString(
      version_info->DeviceFamily);

  if (family.compare("Windows.Desktop") == 0) {
    return kSbSystemDeviceTypeDesktopPC;
  }
  if (family.compare("Windows.Xbox") == 0) {
    return kSbSystemDeviceTypeGameConsole;
  }
  SB_NOTREACHED();
  return kSbSystemDeviceTypeUnknown;
}

#endif
