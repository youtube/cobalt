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

#include <stdlib.h>

#include <algorithm>
#include <string>
#include <vector>

#include "internal/starboard/xb1/shared/internal_shims.h"
#include "starboard/common/device_type.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/keyboxes/xbox/system_properties.h"
#include "starboard/memory.h"
#include "starboard/shared/uwp/application_uwp.h"
#include "starboard/shared/uwp/keys.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/system.h"

using starboard::shared::win32::platformStringToString;
using Windows::Security::ExchangeActiveSyncProvisioning::
    EasClientDeviceInformation;
using Windows::System::Profile::AnalyticsInfo;
using Windows::System::Profile::AnalyticsVersionInfo;
using Windows::System::UserProfile::AdvertisingManager;

namespace {

#define arraysize(array) (sizeof(array) / sizeof(*array))

struct UwpDevice {
  const char* deviceForm;
  const char* chipsetModel;
  const char* model;
  const char* year;
} UwpDevices;

// Array of model name and year for known UWP devices.
struct UwpDevice kDevices[] = {
    {"Xbox One", "XboxOne", "XboxOne", "2013"},
    {"Xbox One S", "XboxOne", "XboxOne S", "2016"},
    {"Xbox One X", "XboxOne", "XboxOne X", "2017"},
    {"Xbox One X DevKit", "XboxOne", "XboxOne X", "2017"},
    {"Xbox Series X", "XboxScarlett", "XboxScarlett Series X", "2020"},
    {"Xbox Series X Devkit", "XboxScarlett", "XboxScarlett Series X", "2020"},
    {"Xbox Series S", "XboxScarlett", "XboxScarlett Series S", "2020"},
};

const char kSystemIntegrator[] = "YouTube";
const char kXboxDeviceFormField[] = "Xbox";

// Year for unknown Uwp devices. This assumes that they will be as
// capable as the most recent known device.
const char kUnknownModelYear[] = "2020";

// Chipset for unidentified device forms.
const char kUnknownChipset[] = "UwpUnknown";

// XOR key for certification secret.
const char kRandomKey[] = "27539";

bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (strlen(from_value) + 1 > value_length)
    return false;
  starboard::strlcpy(out_value, from_value, value_length);
  return true;
}

bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const wchar_t* from_value) {
  char* from_value_str = new char[value_length];
  int len = wcstombs(from_value_str, from_value, value_length);
  bool result = len < 0 ? false
                        : CopyStringAndTestIfSuccess(out_value, value_length,
                                                     from_value_str);
  delete from_value_str;
  return result;
}

bool StartsWith(const std::string& str, const char* prefix) {
  size_t len = strlen(prefix);
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
  AnalyticsVersionInfo ^ version_info = AnalyticsInfo::VersionInfo;
  std::string device_family_version =
      platformStringToString(version_info->DeviceFamilyVersion);
  if (device_family_version.empty()) {
    return false;
  }
  uint64_t version_info_all =
      strtoull(device_family_version.c_str(), nullptr, 10);
  if (version_info_all == 0) {
    return false;
  }
  version->major_version = (version_info_all >> 48) & 0xFFFF;
  version->minor_version = (version_info_all >> 32) & 0xFFFF;
  version->build_version = (version_info_all >> 16) & 0xFFFF;
  version->revision = version_info_all & 0xFFFF;
  return true;
}

std::string GetDeviceForm() {
  Platform::String ^ device_form = AnalyticsInfo::DeviceForm;
  return platformStringToString(device_form);
}

bool GetBrandName(char* out_value, int value_length) {
  EasClientDeviceInformation ^ current_device_info =
      ref new EasClientDeviceInformation();
  std::string brand_name =
      platformStringToString(current_device_info->SystemManufacturer);
  if (brand_name.empty()) {
    return false;
  }
  return CopyStringAndTestIfSuccess(out_value, value_length,
                                    brand_name.c_str());
}

bool GetChipsetModelNumber(char* out_value, int value_length) {
  std::string deviceForm = GetDeviceForm();
  // If the device form is a known device, return the chipset model from the
  // table.
  for (size_t i = 0; i < arraysize(kDevices); i++) {
    const UwpDevice* device = kDevices + i;
    if (deviceForm == device->deviceForm) {
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        device->chipsetModel);
    }
  }

  // The device form is not a known value, return unknown chipset.
  return CopyStringAndTestIfSuccess(out_value, value_length, kUnknownChipset);
}

bool GetFirmwareVersion(char* out_value, int value_length) {
  WindowsVersion version = {0};
  if (!GetWindowsVersion(&version)) {
    return false;
  }
  // The caller expects that the the output string will only be written if
  // when true is returned. Therefore we have to buffer the string in case
  // there is a false condition, such as small memory input size to hold
  // the output parameter.
  std::vector<char> out_path_copy(kSbFileMaxPath + 1, 0);
  int len = std::min<int>(kSbFileMaxPath, value_length);
  int return_value = SbStringFormatF(
      out_path_copy.data(), len, "%u.%u.%u.%u", version.major_version,
      version.minor_version, version.build_version, version.revision);

  const bool ok = ((return_value > 0) && (return_value < value_length));
  if (ok) {
    starboard::strlcpy(out_value, out_path_copy.data(), len);
  }
  return ok;
}

bool GetFriendlyName(char* out_value, int value_length) {
  EasClientDeviceInformation ^ current_device_info =
      ref new EasClientDeviceInformation();
  std::string friendly_name =
      platformStringToString(current_device_info->FriendlyName);
  if (friendly_name.empty()) {
    return false;
  }
  return CopyStringAndTestIfSuccess(out_value, value_length,
                                    friendly_name.c_str());
}

bool GetModelYear(char* out_value, int value_length) {
  std::string deviceForm = GetDeviceForm();
  for (size_t i = 0; i < arraysize(kDevices); i++) {
    const UwpDevice* device = kDevices + i;
    if (deviceForm == device->deviceForm) {
      return CopyStringAndTestIfSuccess(out_value, value_length, device->year);
    }
  }

  return CopyStringAndTestIfSuccess(out_value, value_length, kUnknownModelYear);
}

bool GetModelName(char* out_value, int value_length) {
  std::string deviceForm = GetDeviceForm();
  for (size_t i = 0; i < arraysize(kDevices); i++) {
    const UwpDevice* device = kDevices + i;
    if (deviceForm == device->deviceForm) {
      return CopyStringAndTestIfSuccess(out_value, value_length, device->model);
    }
  }

  // The device form is not a known value, return the device form verbatim.
  return CopyStringAndTestIfSuccess(out_value, value_length,
                                    deviceForm.c_str());
}

bool GetPlatformName(char* out_value, int value_length) {
  EasClientDeviceInformation ^ current_device_info =
      ref new EasClientDeviceInformation();
  std::string operating_system =
      platformStringToString(current_device_info->OperatingSystem);

  AnalyticsVersionInfo ^ version_info = AnalyticsInfo::VersionInfo;
  std::string os_name_and_version =
      platformStringToString(current_device_info->OperatingSystem);
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

bool GetAppXVersion(char* out_value, int value_length) {
  Windows::ApplicationModel::PackageVersion version =
      Windows::ApplicationModel::Package::Current->Id->Version;
  std::stringstream version_string;
  version_string << version.Major << '.' << version.Minor << '.'
                 << version.Build << '.' << version.Revision;
  return CopyStringAndTestIfSuccess(out_value, value_length,
                                    version_string.str().c_str());
}

size_t XorString(char* dest, const char* src) {
  size_t keyLen = strlen(kRandomKey);
  size_t ret = strlen(src);
  for (size_t i = 0; i < ret; i++) {
    dest[i] = src[i] ^ (kRandomKey[i % keyLen]);
  }
  return ret;
}

std::string GetAdvertisingId() {
  Platform::String ^ advertising_id = AdvertisingManager::AdvertisingId;
  return platformStringToString(advertising_id);
}

bool GetDeviceType(char* out_value, int value_length) {
  AnalyticsVersionInfo ^ version_info = AnalyticsInfo::VersionInfo;
  std::string family = starboard::shared::win32::platformStringToString(
      version_info->DeviceFamily);
  std::string device_type;
  if (family.compare("Windows.Desktop") == 0) {
    return CopyStringAndTestIfSuccess(out_value, value_length,
                                      starboard::kSystemDeviceTypeDesktopPC);
  }
  if (family.compare("Windows.Xbox") == 0) {
    return CopyStringAndTestIfSuccess(out_value, value_length,
                                      starboard::kSystemDeviceTypeGameConsole);
  }
  SB_NOTREACHED();
  return CopyStringAndTestIfSuccess(out_value, value_length,
                                    starboard::kSystemDeviceTypeUnknown);
}

}  // namespace

bool SbSystemGetProperty(SbSystemPropertyId property_id,
                         char* out_value,
                         int value_length) {
  if (!out_value || !value_length) {
    return false;
  }

  using starboard::shared::uwp::SpeechApiKey;

  switch (property_id) {
    case kSbSystemPropertyCertificationScope: {
      Platform::String ^ scope = starboard::xb1::shared::GetCertScope();
      if (scope->IsEmpty()) {
        if (kCertificationScope[0] == '\0')
          return false;
        return CopyStringAndTestIfSuccess(out_value, value_length,
                                          kCertificationScope);
      }
      bool result =
          CopyStringAndTestIfSuccess(out_value, value_length, scope->Data());
      return result;
    }
#if SB_API_VERSION < 13
    case kSbSystemPropertyBase64EncodedCertificationSecret: {
      if (kBase64EncodedCertificationSecret0[0] == '\0')
        return false;
      char secret_buffer[sizeof(kBase64EncodedCertificationSecret0) +
                         sizeof(kBase64EncodedCertificationSecret1)];
      memset(secret_buffer, 0, sizeof(secret_buffer));
      size_t len = XorString(secret_buffer, kBase64EncodedCertificationSecret0);
      XorString(secret_buffer + len, kBase64EncodedCertificationSecret1);
      return CopyStringAndTestIfSuccess(out_value, value_length, secret_buffer);
    }
#endif  // SB_API_VERSION < 13
    case kSbSystemPropertyChipsetModelNumber:
      return GetChipsetModelNumber(out_value, value_length);
    case kSbSystemPropertyFirmwareVersion:
      return GetFirmwareVersion(out_value, value_length);
    case kSbSystemPropertyFriendlyName:
      return GetFriendlyName(out_value, value_length);
    case kSbSystemPropertyBrandName:
      return GetBrandName(out_value, value_length);
    case kSbSystemPropertyModelName:
      return GetModelName(out_value, value_length);
    case kSbSystemPropertyModelYear:
      return GetModelYear(out_value, value_length);
    case kSbSystemPropertySystemIntegratorName:
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        kSystemIntegrator);
    case kSbSystemPropertyPlatformName:
      return GetPlatformName(out_value, value_length);
    case kSbSystemPropertySpeechApiKey:
      CopyStringAndTestIfSuccess(out_value, value_length, SpeechApiKey());
      return true;
    case kSbSystemPropertyUserAgentAuxField:
      return GetAppXVersion(out_value, value_length);
#if SB_API_VERSION >= 14
    case kSbSystemPropertyAdvertisingId: {
      std::string advertising_id = GetAdvertisingId();
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        advertising_id.c_str());
    }
    case kSbSystemPropertyLimitAdTracking: {
      std::string advertising_id = GetAdvertisingId();
      // If we get an empty ID, that means the user disabled it.
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        advertising_id.empty() ? "1" : "0");
    }
#endif  // SB_API_VERSION >= 14
#if SB_API_VERSION >= SB_SYSTEM_DEVICE_TYPE_AS_STRING_API_VERSION
    case kSbSystemPropertyDeviceType:
      return GetDeviceType(out_value, value_length);
#endif
    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }
  return false;
}
