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

#include <sys/utsname.h>

#include <string>

#if SB_API_VERSION >= 11
#include "starboard/format_string.h"
#endif  // SB_API_VERSION >= 11
#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace {

const char* kFriendlyName = "Raspberry Pi";

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
#if SB_API_VERSION >= 11
    case kSbSystemPropertyOriginalDesignManufacturerName:
#else
    case kSbSystemPropertyNetworkOperatorName:
#endif
    case kSbSystemPropertySpeechApiKey:
      return false;

    case kSbSystemPropertyFriendlyName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kFriendlyName);

    case kSbSystemPropertyPlatformName: {
      // Example output: "Raspian Linux armv7l".
      utsname name;

      if (uname(&name) == -1)
        return false;

      std::string temp =
          starboard::FormatString("Raspian %s %s", name.sysname, name.machine);

      return CopyStringAndTestIfSuccess(out_value, value_length, temp.c_str());
    }

    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }

  return false;
}
