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

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/shared/environment.h"
#include "starboard/system.h"

namespace {

const char* kFriendlyName = "Windows Desktop";
const char* kPlatformName = "win32; Windows x86_64";

bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (strlen(from_value) + 1 > value_length)
    return false;
  starboard::strlcpy(out_value, from_value, value_length);
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
    case kSbSystemPropertySystemIntegratorName:
    case kSbSystemPropertySpeechApiKey:
      return false;

    case kSbSystemPropertyFriendlyName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kFriendlyName);

    case kSbSystemPropertyPlatformName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kPlatformName);

#if SB_API_VERSION >= 14
    // Implementation provided for testing purposes only
    case kSbSystemPropertyAdvertisingId:
      return CopyStringAndTestIfSuccess(
          out_value, value_length,
          starboard::GetEnvironment("COBALT_ADVERTISING_ID").c_str());
    case kSbSystemPropertyLimitAdTracking:
      return CopyStringAndTestIfSuccess(
          out_value, value_length,
          starboard::GetEnvironment("COBALT_LIMIT_AD_TRACKING").c_str());
#endif

    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }

  return false;
}
