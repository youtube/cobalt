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

#include <netdb.h>
#include <linux/if.h>  // NOLINT(build/include_alpha)
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/linux/x64x11/system_properties.h"

namespace {

const char* kFriendlyName = "Linux Desktop";
const char* kPlatformName = "X11; Linux x86_64";

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

  switch (property_id) {
    case kSbSystemPropertyBrandName:
    case kSbSystemPropertyChipsetModelNumber:
    case kSbSystemPropertyFirmwareVersion:
    case kSbSystemPropertyModelName:
    case kSbSystemPropertyModelYear:
#if SB_API_VERSION >= 12
    case kSbSystemPropertySystemIntegratorName:
#else
    case kSbSystemPropertyOriginalDesignManufacturerName:
#endif
    case kSbSystemPropertySpeechApiKey:
      return false;
    case kSbSystemPropertyUserAgentAuxField:
      return false;
    case kSbSystemPropertyFriendlyName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kFriendlyName);

    case kSbSystemPropertyPlatformName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kPlatformName);

    case kSbSystemPropertyCertificationScope:
      if (kCertificationScope[0] == '\0')
        return false;
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        kCertificationScope);


    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }

  return false;
}

}  // namespace x64x11
}  // namespace starboard
