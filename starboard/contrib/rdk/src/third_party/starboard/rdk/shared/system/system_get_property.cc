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
// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>

#include <ctype.h>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/file.h"

#include "third_party/starboard/rdk/shared/platform/platform_interface.h"
#include "third_party/starboard/rdk/shared/system/system_properties_override.h"
#include "third_party/starboard/rdk/shared/log_override.h"

using namespace third_party::starboard::rdk::shared;
using namespace third_party::starboard::rdk::shared::system;

namespace {

const char kPlatformName[] = "RDK";

bool CopyStringAndTestIfSuccess(char* out_value,
                                size_t value_length,
                                const char* from_value) {
  if (!from_value || strlen(from_value) + 1 > value_length)
    return false;
  ::starboard::strlcpy(out_value, from_value, value_length);
  return true;
}

bool GetFriendlyName(char* out_value, int value_length) {
  if (std::string prop; SystemProperties::GetFriendlyName(prop)) {
    return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
  }

#if defined(SB_PLATFORM_FRIENDLY_NAME)
  return CopyStringAndTestIfSuccess(out_value, value_length,
                                    SB_PLATFORM_FRIENDLY_NAME);
#endif  // defined(SB_PLATFORM_FRIENDLY_NAME)

  return false;
}

bool GetModelYear(char* out_value, int value_length) {
  if (std::string prop; SystemProperties::GetModelYear(prop)) {
    return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
  }

#if defined(SB_PLATFORM_MODEL_YEAR)
  return CopyStringAndTestIfSuccess(out_value, value_length,
    std::to_string(SB_PLATFORM_MODEL_YEAR).c_str());
#endif  // defined(SB_PLATFORM_MODEL_YEAR)

  return false;
}

bool GetModelName(char* out_value, int value_length) {
  if (std::string prop; SystemProperties::GetModelName(prop)) {
    return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
  }

  if (const char* env = std::getenv("COBALT_MODEL_NAME"); env != nullptr) {
    return CopyStringAndTestIfSuccess(out_value, value_length, env);
  }

  return CopyStringAndTestIfSuccess(out_value, value_length,
    SB_PLATFORM_MODEL_NAME);
}

bool GetBrandName(char* out_value, int value_length) {
  if (std::string brand_name; SystemProperties::GetBrandName(brand_name)) {
    return CopyStringAndTestIfSuccess(out_value, value_length, brand_name.c_str());
  }

  if (auto brand_name = platform::device().brand_name(); brand_name.has_value()) {
    return CopyStringAndTestIfSuccess(out_value, value_length, brand_name->c_str());
  }

  if (const char* env = std::getenv("COBALT_OPERATOR_NAME"); env != nullptr) {
    return CopyStringAndTestIfSuccess(out_value, value_length, env);
  }

  return CopyStringAndTestIfSuccess(out_value, value_length,
    SB_PLATFORM_OPERATOR_NAME);
}

bool GetManufacturerName(char* out_value, int value_length) {
  if (std::string prop; SystemProperties::GetIntegratorName(prop)) {
    return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
  }

  if (const char* env = std::getenv("COBALT_MANUFACTURE_NAME"); env != nullptr) {
    return CopyStringAndTestIfSuccess(out_value, value_length, env);
  }

#if defined(SB_PLATFORM_MANUFACTURER_NAME)
  return CopyStringAndTestIfSuccess(out_value, value_length,
    SB_PLATFORM_MANUFACTURER_NAME);
#endif  // defined(SB_PLATFORM_MANUFACTURER_NAME)

  return false;
}

bool GetChipsetModelNumber(char* out_value, int value_length) {
  if (std::string chipset; SystemProperties::GetChipset(chipset)) {
    return CopyStringAndTestIfSuccess(out_value, value_length, chipset.c_str());
  }

  if (auto chipset = platform::device().chipset(); chipset.has_value()) {
    return CopyStringAndTestIfSuccess(out_value, value_length, chipset->c_str());
  }

  return false;
}

bool GetFirmwareVersion(char* out_value, int value_length) {
  if (std::string firmware_version; SystemProperties::GetFirmwareVersion(firmware_version)) {
    return CopyStringAndTestIfSuccess(out_value, value_length, firmware_version.c_str());
  }

  if (auto firmware_version = platform::device().firmware_version(); firmware_version.has_value()) {
    return CopyStringAndTestIfSuccess(out_value, value_length, firmware_version->c_str());
  }

  return false;
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
  return ::starboard::strlcpy(out_value, buf.data(), value_length);
}

bool GetLimitAdTracking(char* out_value, int value_length) {
  if (std::string prop; AdvertisingId::GetLmtAdTracking(prop)) {
    return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
  }

  if (auto ifa = platform::advertising().advertising_id(); ifa.has_value() && !ifa->lmt.empty()) {
    return CopyStringAndTestIfSuccess(out_value, value_length, ifa->lmt.c_str());
  }

  return false;
}

bool GetAdvertisingId(char* out_value, int value_length) {
  if (std::string prop; AdvertisingId::GetIfa(prop)) {
    return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
  }

  if (auto ifa = platform::advertising().advertising_id(); ifa.has_value() && !ifa->ifa.empty()) {
    return CopyStringAndTestIfSuccess(out_value, value_length, ifa->ifa.c_str());
  }

  return false;
}

bool GetDeviceType(char* out_value, int value_length) {
  if (std::string prop; SystemProperties::GetDeviceType(prop)) {
    return CopyStringAndTestIfSuccess(out_value, value_length, prop.c_str());
  }

  if (auto device_type = platform::device().device_type(); device_type.has_value()) {
    return CopyStringAndTestIfSuccess(out_value, value_length, device_type->c_str());
  }

  return CopyStringAndTestIfSuccess(out_value, value_length, "STB");
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
      return GetBrandName(out_value, value_length);

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
