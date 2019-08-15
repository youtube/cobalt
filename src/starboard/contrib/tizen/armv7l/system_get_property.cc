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

#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace {

const char* kBrandName = "Samsung";
const char* kChipsetModelNumber = "XU3";
const char* kFirmwareVersion = "tv-wayland-armv7l-odroidu3";
const char* kFriendlyName = "Tizen";
const char* kModelName = "ODROID";
const char* kModelYear = "2018";
const char* kPlatformName = "LINUX; Tizen 3.0;";

bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (SbStringGetLength(from_value) + 1 > value_length)
    return false;
  SbStringCopy(out_value, from_value, value_length);
  return true;
}

}  // namespace

static char* model_number;

bool SbSystemGetProperty(SbSystemPropertyId property_id,
                         char* out_value,
                         int value_length) {
  if (!out_value || !value_length) {
    return false;
  }

  switch (property_id) {
    case kSbSystemPropertyBrandName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kBrandName);
    case kSbSystemPropertyChipsetModelNumber:
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        kChipsetModelNumber);
    case kSbSystemPropertyFirmwareVersion:
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        kFirmwareVersion);
    case kSbSystemPropertyModelName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kModelName);
    case kSbSystemPropertyModelYear:
      return CopyStringAndTestIfSuccess(out_value, value_length, kModelYear);
#if SB_API_VERSION >= 11
    case kSbSystemPropertyOriginalDesignManufacturerName:
#else
    case kSbSystemPropertyNetworkOperatorName:
#endif
      return false;

    case kSbSystemPropertyFriendlyName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kFriendlyName);

    case kSbSystemPropertyPlatformName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kPlatformName);

    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }

  return false;
}
