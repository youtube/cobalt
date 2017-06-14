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

#include "starboard/system.h"

#include <string>

#include "starboard/log.h"
#include "starboard/shared/uwp/application_uwp.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/string.h"

using Windows::Security::ExchangeActiveSyncProvisioning::
    EasClientDeviceInformation;
using Windows::System::Profile::AnalyticsInfo;
using Windows::System::Profile::AnalyticsVersionInfo;

namespace {
bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (SbStringGetLength(from_value) + 1 > value_length)
    return false;
  SbStringCopy(out_value, from_value, value_length);
  return true;
}
}  // namespace

bool SbSystemGetProperty(SbSystemPropertyId property_id,
                         char* out_value,
                         int value_length) {
  if (!out_value || !value_length) {
    return false;
  }

  switch (property_id) {
    case kSbSystemPropertyBrandName:
    case kSbSystemPropertyChipsetModelNumber:
    case kSbSystemPropertyFirmwareVersion:
    case kSbSystemPropertyModelName:
    case kSbSystemPropertyModelYear:
    case kSbSystemPropertyNetworkOperatorName:
    case kSbSystemPropertySpeechApiKey:
      return false;

    case kSbSystemPropertyFriendlyName: {
      EasClientDeviceInformation^ current_device_info =
          ref new EasClientDeviceInformation();
      std::string friendly_name =
          starboard::shared::win32::platformStringToString(
            current_device_info->FriendlyName);
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        friendly_name.c_str());
    }

    case kSbSystemPropertyPlatformName: {
      AnalyticsVersionInfo^ version_info = AnalyticsInfo::VersionInfo;
      std::string platform_str =
          starboard::shared::win32::platformStringToString(
            version_info->DeviceFamily);
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        platform_str.c_str());
    }
    case kSbSystemPropertyPlatformUuid: {
      SB_NOTIMPLEMENTED();
      return CopyStringAndTestIfSuccess(out_value, value_length, "N/A");
    }
    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }
  return false;
}
