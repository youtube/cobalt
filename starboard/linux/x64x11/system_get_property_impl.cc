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

#include "starboard/linux/x64x11/system_get_property_impl.h"

#include <string>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/linux/x64x11/system_properties.h"
#include "starboard/shared/environment.h"

namespace {

const char kBrandName[] = "BrandName";
const char kChipsetModelNumber[] = "ChipsetModelNumber";
const char kFirmwareVersion[] = "FirmwareVersion";
const char kFriendlyName[] = "Linux Desktop";
const char kModelName[] = "ModelName";
const char kPlatformName[] = "X11; Linux x86_64";
const char kSystemIntegratorName[] = "SystemIntegratorName";

#if SB_API_VERSION >= 14
const char kModelYear[] = "2023";
#elif SB_API_VERSION >= 13
const char kModelYear[] = "2022";
#elif SB_API_VERSION >= 12
const char kModelYear[] = "2021";
#endif  // SB_API_VERSION
}  // namespace

// Omit namespace linux due to symbol name conflict.
namespace starboard {
namespace x64x11 {

bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (strlen(from_value) + 1 > value_length)
    return false;
  starboard::strlcpy(out_value, from_value, value_length);
  return true;
}

bool GetSystemProperty(SbSystemPropertyId property_id,
                       char* out_value,
                       int value_length) {
  if (!out_value || !value_length) {
    return false;
  }

  std::string env_value;
  switch (property_id) {
    case kSbSystemPropertyBrandName:
      env_value = GetEnvironment("COBALT_TESTING_BRAND_NAME");
      return CopyStringAndTestIfSuccess(
          out_value, value_length,
          env_value.empty() ? kBrandName : env_value.c_str());
    case kSbSystemPropertyCertificationScope:
      if (kCertificationScope[0] == '\0')
        return false;
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        kCertificationScope);
    case kSbSystemPropertyChipsetModelNumber:
      env_value = GetEnvironment("COBALT_TESTING_CHIPSET_MODEL_NUMBER");
      return CopyStringAndTestIfSuccess(
          out_value, value_length,
          env_value.empty() ? kChipsetModelNumber : env_value.c_str());
    case kSbSystemPropertyFirmwareVersion:
      env_value = GetEnvironment("COBALT_TESTING_FIRMWARE_VERSION");
      return CopyStringAndTestIfSuccess(
          out_value, value_length,
          env_value.empty() ? kFirmwareVersion : env_value.c_str());
    case kSbSystemPropertyFriendlyName:
      env_value = GetEnvironment("COBALT_TESTING_FRIENDLY_NAME");
      return CopyStringAndTestIfSuccess(
          out_value, value_length,
          env_value.empty() ? kFriendlyName : env_value.c_str());
    case kSbSystemPropertyModelName:
      env_value = GetEnvironment("COBALT_TESTING_MODEL_NAME");
      return CopyStringAndTestIfSuccess(
          out_value, value_length,
          env_value.empty() ? kModelName : env_value.c_str());
    case kSbSystemPropertyModelYear:
      env_value = GetEnvironment("COBALT_TESTING_MODEL_YEAR");
      return CopyStringAndTestIfSuccess(
          out_value, value_length,
          env_value.empty() ? kModelYear : env_value.c_str());
    case kSbSystemPropertyPlatformName:
      env_value = GetEnvironment("COBALT_TESTING_PLATFORM_NAME");
      return CopyStringAndTestIfSuccess(
          out_value, value_length,
          env_value.empty() ? kPlatformName : env_value.c_str());
    case kSbSystemPropertySystemIntegratorName:
      env_value = GetEnvironment("COBALT_TESTING_SYSTEM_INTEGRATOR_NAME");
      return CopyStringAndTestIfSuccess(
          out_value, value_length,
          env_value.empty() ? kSystemIntegratorName : env_value.c_str());
    case kSbSystemPropertySpeechApiKey:
    case kSbSystemPropertyUserAgentAuxField:
      return false;
#if SB_API_VERSION >= 14
    // Implementation provided for testing purposes only
    case kSbSystemPropertyAdvertisingId:
      return CopyStringAndTestIfSuccess(
          out_value, value_length,
          GetEnvironment("COBALT_ADVERTISING_ID").c_str());
    case kSbSystemPropertyLimitAdTracking:
      return CopyStringAndTestIfSuccess(
          out_value, value_length,
          GetEnvironment("COBALT_LIMIT_AD_TRACKING").c_str());
#endif
    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }

  return false;
}

}  // namespace x64x11
}  // namespace starboard
