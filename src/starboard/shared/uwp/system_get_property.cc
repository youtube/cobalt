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
#include "starboard/shared/uwp/private/keys.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/string.h"

using Windows::Security::ExchangeActiveSyncProvisioning::
    EasClientDeviceInformation;
using Windows::System::Profile::AnalyticsInfo;
using Windows::System::Profile::AnalyticsVersionInfo;

namespace sbwin32 = starboard::shared::win32;

namespace {
bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (SbStringGetLength(from_value) + 1 > value_length)
    return false;
  SbStringCopy(out_value, from_value, value_length);
  return true;
}

bool StartsWith(const std::string& str, const char* prefix) {
  size_t len = SbStringGetLength(prefix);
  if (str.size() < len) {
    return false;
  }

  return 0 == str.compare(0, len, prefix);
}

const std::size_t kOsVersionSize = 128;

struct WindowsVersion {
  uint16_t major_version;
  uint16_t minor_version;
  uint16_t build_version;
  uint16_t revision;
};

bool GetWindowsVersion(WindowsVersion* version) {
  SB_DCHECK(version);
  AnalyticsVersionInfo^ version_info = AnalyticsInfo::VersionInfo;
  std::string device_family_version =
      starboard::shared::win32::platformStringToString(
          version_info->DeviceFamilyVersion);
  if (device_family_version.empty()) {
    return false;
  }
  uint64_t version_info_all =
      SbStringParseUInt64(device_family_version.c_str(), nullptr, 10);
  if (version_info_all == 0) {
    return false;
  }
  version->major_version = (version_info_all >> 48) & 0xFFFF;
  version->minor_version = (version_info_all >> 32) & 0xFFFF;
  version->build_version = (version_info_all >> 16) & 0xFFFF;
  version->revision = version_info_all & 0xFFFF;
  return true;
}

const char kXboxOneSkuPrefix[] = "XBOX_ONE_";

}  // namespace

bool SbSystemGetProperty(SbSystemPropertyId property_id,
                         char* out_value,
                         int value_length) {
  if (!out_value || !value_length) {
    return false;
  }

  using sbwin32::platformStringToString;
  using starboard::shared::uwp::SpeechApiKey;

  switch (property_id) {
    case kSbSystemPropertyModelYear:
    case kSbSystemPropertyNetworkOperatorName:
    case kSbSystemPropertyUserAgentAuxField:
      return false;
    case kSbSystemPropertySpeechApiKey:
      CopyStringAndTestIfSuccess(out_value, value_length, SpeechApiKey());
      return true;
    case kSbSystemPropertyBrandName: {
      EasClientDeviceInformation^ current_device_info =
          ref new EasClientDeviceInformation();
      std::string brand_name =
          platformStringToString(current_device_info->SystemManufacturer);
      if (brand_name.empty()) {
        return false;
      }
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        brand_name.c_str());
    }
    case kSbSystemPropertyFirmwareVersion: {
      WindowsVersion version = {0};
      if (!GetWindowsVersion(&version)) {
        return false;
      }
      int return_value = SbStringFormatF(
          out_value, value_length, "%u.%u.%u.%u", version.major_version,
          version.minor_version, version.build_version, version.revision);
      return ((return_value > 0) && (return_value < value_length));
    }
    case kSbSystemPropertyChipsetModelNumber: {
      std::string sku = platformStringToString(
          (ref new EasClientDeviceInformation())->SystemSku);

      std::string result;
      if (StartsWith(sku, kXboxOneSkuPrefix)) {
        result = "XboxOne";
      } else {
        result = sku;
      }

      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        result.c_str());
    }
    case kSbSystemPropertyModelName: {
      std::string sku = platformStringToString(
          (ref new EasClientDeviceInformation())->SystemSku);

      std::string friendly_name;

      if (sku == "XBOX_ONE_DU") {
        friendly_name = "XboxOne";
      } else if (sku == "XBOX_ONE_ED") {
        friendly_name = "XboxOne S";
      } else if (sku == "XBOX_ONE_CH" || sku == "XBOX_ONE_SC") {
        friendly_name = "XboxOne X";
      } else if (StartsWith(sku, kXboxOneSkuPrefix)) {
        friendly_name = "XboxOne " + sku;
      } else {
        friendly_name = sku;
      }

      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        friendly_name.c_str());
    }
    case kSbSystemPropertyFriendlyName: {
      EasClientDeviceInformation^ current_device_info =
          ref new EasClientDeviceInformation();
      std::string friendly_name =
          platformStringToString(current_device_info->FriendlyName);
      if (friendly_name.empty()) {
        return false;
      }
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        friendly_name.c_str());
    }
    case kSbSystemPropertyPlatformName: {
      EasClientDeviceInformation^ current_device_info =
          ref new EasClientDeviceInformation();
      std::string operating_system =
          platformStringToString(current_device_info->OperatingSystem);

      AnalyticsVersionInfo^ version_info = AnalyticsInfo::VersionInfo;
      std::string os_name_and_version =
          starboard::shared::win32::platformStringToString(
              current_device_info->OperatingSystem);
      if (os_name_and_version.empty()) {
        return false;
      }

      WindowsVersion os_version;
      if (!GetWindowsVersion(&os_version)) {
        return false;
      }

      os_name_and_version += " ";
      char os_version_buffer[kOsVersionSize];
      os_version_buffer[0] = '\0';

      int return_value =
          SbStringFormatF(os_version_buffer, value_length, "%u.%u",
                          os_version.major_version, os_version.minor_version);
      if ((return_value < 0) || (return_value >= value_length)) {
        return false;
      }

      os_name_and_version.append(os_version_buffer);

      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        os_name_and_version.c_str());
    }
#if SB_API_VERSION < SB_PROPERTY_UUID_REMOVED_API_VERSION
    case kSbSystemPropertyPlatformUuid: {
      SB_NOTIMPLEMENTED();
      return CopyStringAndTestIfSuccess(out_value, value_length, "N/A");
    }
#endif  // SB_API_VERSION < SB_PROPERTY_UUID_REMOVED_API_VERSION
    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }
  return false;
}
