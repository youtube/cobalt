//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
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

#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>

#include <ctype.h>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/file.h"

#include "third_party/starboard/rdk/shared/rdkservices.h"
#include "third_party/starboard/rdk/shared/log_override.h"

using namespace third_party::starboard::rdk::shared;

namespace {

const char kPlatformName[] = "Linux";

bool CopyStringAndTestIfSuccess(char* out_value,
                                size_t value_length,
                                const char* from_value) {
  if (strlen(from_value) + 1 > value_length)
    return false;
  starboard::strlcpy<char>(out_value, from_value, value_length);
  return true;
}

bool TryReadFromPropertiesFile(const char* prefix, size_t prefix_len, char* out_value, size_t value_length) {
  FILE* properties = fopen("/etc/device.properties", "r");
  if (!properties) {
    return false;
  }

  bool result = false;
  char* buffer = nullptr;
  size_t size = 0;

  while (getline(&buffer, &size, properties) != -1) {
    if (strncmp(prefix, buffer, prefix_len) == 0) {
      char* remainder = buffer + prefix_len;
      size_t remainder_length = strlen(remainder);
      if (remainder_length > 1 && remainder_length < value_length) {
        // trim the newline character
        for(int i = remainder_length - 1; i >= 0 && !std::isalnum(remainder[i]); --i)
          remainder[i] = '\0';
        std::transform(
          remainder, remainder + remainder_length - 1, remainder,
          [](unsigned char c) -> unsigned char { return toupper(c); } );
        starboard::strlcpy<char>(out_value, remainder, remainder_length);
        result = true;
        break;
      }
    }
  }

  free(buffer);
  fclose(properties);

  return result;
}

bool GetFriendlyName(char* out_value, int value_length) {
  std::string prop;
  if (SystemProperties::GetFriendlyName(prop))
    return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
#if defined(SB_PLATFORM_FRIENDLY_NAME)
  return CopyStringAndTestIfSuccess(out_value, value_length,
                                    SB_PLATFORM_FRIENDLY_NAME);
#endif  // defined(SB_PLATFORM_FRIENDLY_NAME)
  return false;
}

bool GetModelYear(char* out_value, int value_length) {
  std::string prop;
  if (SystemProperties::GetModelYear(prop))
    return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());

#if defined(SB_PLATFORM_MODEL_YEAR)
  return CopyStringAndTestIfSuccess(out_value, value_length,
    std::to_string(SB_PLATFORM_MODEL_YEAR).c_str());
#endif  // defined(SB_PLATFORM_MODEL_YEAR)
  return false;
}

bool GetModelName(char* out_value, int value_length) {
  std::string prop;
  if (SystemProperties::GetModelName(prop))
    return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());

  const char* env = std::getenv("COBALT_MODEL_NAME");
  if (env && CopyStringAndTestIfSuccess(out_value, value_length, env))
    return true;

  const char kPrefixStr[] = "MODEL_NUM=";
  const size_t kPrefixStrLength = SB_ARRAY_SIZE(kPrefixStr) - 1;
  if (TryReadFromPropertiesFile(kPrefixStr, kPrefixStrLength, out_value, value_length)) {
    if (AuthService::GetExperience(prop) && prop == "Flex") {
      starboard::strlcat<char>(out_value, prop.c_str(), value_length);
    }
    return true;
  }

  return CopyStringAndTestIfSuccess(out_value, value_length, SB_PLATFORM_MODEL_NAME);
}

bool GetOperatorName(char* out_value, int value_length) {
  std::string prop;
  if (SystemProperties::GetBrandName(prop) || DeviceInfo::GetBrandName(prop)) {
    return CopyStringAndTestIfSuccess(
      out_value, value_length, prop.c_str());
  }

  const char* env = std::getenv("COBALT_OPERATOR_NAME");
  if (env && CopyStringAndTestIfSuccess(out_value, value_length, env))
    return true;

  FILE* partnerId = fopen("/opt/www/authService/partnerId3.dat", "r");
  if (partnerId) {
    bool result = false;
    char* buffer = nullptr;
    size_t size = 0;
    if (getline(&buffer, &size, partnerId) != -1) {
      // trim the newline character
      for(int i = size - 1; i >= 0 && !std::isalnum(buffer[i]); --i)
        buffer[i] = '\0';
      result = CopyStringAndTestIfSuccess(out_value, value_length, buffer);
    }
    free(buffer);
    fclose(partnerId);
    if (result)
      return true;
  }

  return CopyStringAndTestIfSuccess(out_value, value_length, SB_PLATFORM_OPERATOR_NAME);
}

bool GetManufacturerName(char* out_value, int value_length) {
    std::string prop;
    if (SystemProperties::GetIntegratorName(prop)) {
      return CopyStringAndTestIfSuccess(
        out_value, value_length, prop.c_str());
    }

    const char* env = std::getenv("COBALT_MANUFACTURE_NAME");
    if (env && CopyStringAndTestIfSuccess(out_value, value_length, env))
        return true;

    const char kPrefixStr[] = "MANUFACTURE=";
    const size_t kPrefixStrLength = SB_ARRAY_SIZE(kPrefixStr) - 1;
    if (TryReadFromPropertiesFile(kPrefixStr, kPrefixStrLength, out_value, value_length))
        return true;
#if defined(SB_PLATFORM_MANUFACTURER_NAME)
    return CopyStringAndTestIfSuccess(out_value, value_length,
                                      SB_PLATFORM_MANUFACTURER_NAME);
#else
    return false;
#endif  // defined(SB_PLATFORM_MANUFACTURER_NAME)
}

bool GetChipsetModelNumber(char* out_value, int value_length) {
  std::string chipset;
  if (!SystemProperties::GetChipset(chipset))
    chipset = DeviceIdentification::GetChipset();
  return CopyStringAndTestIfSuccess(
    out_value, value_length, chipset.c_str());
}

bool GetFirmwareVersion(char* out_value, int value_length) {
  std::string firmware_version;
  if (!SystemProperties::GetFirmwareVersion(firmware_version))
    firmware_version = DeviceIdentification::GetFirmwareVersion();

  return CopyStringAndTestIfSuccess(
    out_value, value_length, firmware_version.c_str());
}

bool GetCertificationScope(char* out_value, int value_length) {
  const char *cert_scope_file_name = std::getenv("COBALT_CERT_SCOPE_FILE_NAME");
  if ( cert_scope_file_name == nullptr )
    cert_scope_file_name = "/opt/drm/0681000006810001.bin";

  ::starboard::ScopedFile file(cert_scope_file_name, O_RDONLY);
  if ( !file.IsValid() ) {
    SB_LOG(INFO) << "Cannot open cert scope file '" << cert_scope_file_name << "'";
    return false;
  }

  auto sz = file.GetSize();
  if ( (sz < 0) || (sz + 1 > value_length) ) {
    SB_LOG(ERROR) << "Cannot read cert scope contents of size: " << sz
                  << " from: '" << cert_scope_file_name << "'";
    return false;
  }

  std::vector<char> buf;
  buf.resize(sz + 1);
  if ( file.ReadAll(buf.data(), sz) != sz ) {
    SB_LOG(ERROR) << "Failed to read cert scope contents of size: " << sz
                  << " from: '" << cert_scope_file_name << "'";
    return false;
  }
  buf[sz] = 0;

  SB_LOG(INFO) << "Device cert scope: '" << buf.data() << "'";
  return starboard::strlcpy<char>(out_value, buf.data(), value_length);
}

bool GetLimitAdTracking(char* out_value, int value_length) {
    std::string prop;
    if (AdvertisingId::GetLmtAdTracking(prop)) {
      return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
    }
    return false;
}

bool GetAdvertisingId(char* out_value, int value_length) {
    std::string prop;
    if (AdvertisingId::GetIfa(prop)) {
      return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
    }
    return false;
}

bool GetDeviceType(char* out_value, int value_length) {
    std::string prop;
    if (AuthService::GetExperience(prop) && prop == "Flex") {
      prop = "OTT";
    }
    else if (!SystemProperties::GetDeviceType(prop)) {
      prop = "STB";
    }
    return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
}

}  // namespace

bool SbSystemGetProperty(SbSystemPropertyId property_id,
                         char* out_value,
                         int value_length) {
  if (!out_value || !value_length) {
    return false;
  }

  switch (property_id) {
    case kSbSystemPropertyModelName:
      return GetModelName(out_value, value_length);

    case kSbSystemPropertyBrandName:
      return GetOperatorName(out_value, value_length);

    case kSbSystemPropertyChipsetModelNumber:
      return GetChipsetModelNumber(out_value, value_length);

    case kSbSystemPropertyFirmwareVersion:
      return GetFirmwareVersion(out_value, value_length);

    case kSbSystemPropertyModelYear:
      return GetModelYear(out_value, value_length);

    case kSbSystemPropertySystemIntegratorName:
      return GetManufacturerName(out_value, value_length);

    case kSbSystemPropertySpeechApiKey:
      return false;

    case kSbSystemPropertyFriendlyName:
      return GetFriendlyName(out_value, value_length);

    case kSbSystemPropertyPlatformName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kPlatformName);

    case kSbSystemPropertyCertificationScope:
      return GetCertificationScope(out_value, value_length);

    case kSbSystemPropertyAdvertisingId:
      return GetAdvertisingId(out_value, value_length);

    case kSbSystemPropertyLimitAdTracking:
      return GetLimitAdTracking(out_value, value_length);

    case kSbSystemPropertyDeviceType:
      return GetDeviceType(out_value, value_length);

    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }

  return false;
}
