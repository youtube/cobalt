// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "sys/system_properties.h"

#ifdef SB_HAS_SPEECH_API_KEY
#include "starboard/android/shared/private/keys.h"
#endif  // SB_HAS_SPEECH_API_KEY

#include "starboard/log.h"
#include "starboard/string.h"

// We can't #include "base/stringize_macros.h" in Starboard
#define STRINGIZE_NO_EXPANSION(x) #x
#define STRINGIZE(x) STRINGIZE_NO_EXPANSION(x)

namespace {

const char kFriendlyName[] = "Android";
const char kPlatformName[] = "Android; Linux " STRINGIZE(ANDROID_ABI);
const char kUnknownValue[] = "unknown";

bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (SbStringGetLength(from_value) + 1 > value_length)
    return false;
  SbStringCopy(out_value, from_value, value_length);
  return true;
}

bool GetAndroidSystemProperty(const char* system_property_name,
                              char* out_value,
                              int value_length,
                              const char* default_value) {
  if (value_length < PROP_VALUE_MAX) {
    return false;
  }
  // Note that __system_property_get returns empty string on no value
  __system_property_get(system_property_name, out_value);

  if (strlen(out_value) == 0) {
    SbStringCopy(out_value, default_value, value_length);
  }
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
      return GetAndroidSystemProperty("ro.product.manufacturer", out_value,
                                      value_length, kUnknownValue);
    case kSbSystemPropertyModelName:
      return GetAndroidSystemProperty("ro.product.model", out_value,
                                      value_length, kUnknownValue);
    case kSbSystemPropertyFirmwareVersion:
      return GetAndroidSystemProperty("ro.build.id", out_value, value_length,
                                      kUnknownValue);
    case kSbSystemPropertyChipsetModelNumber:
      return GetAndroidSystemProperty("ro.board.platform", out_value,
                                      value_length, kUnknownValue);
    case kSbSystemPropertyModelYear:
    case kSbSystemPropertyNetworkOperatorName:
      return false;

    case kSbSystemPropertyFriendlyName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kFriendlyName);

    case kSbSystemPropertyPlatformName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kPlatformName);

    case kSbSystemPropertyPlatformUuid:
      SB_NOTIMPLEMENTED();
      return CopyStringAndTestIfSuccess(out_value, value_length, "N/A");

    case kSbSystemPropertySpeechApiKey:
#ifdef SB_HAS_SPEECH_API_KEY
      return CopyStringAndTestIfSuccess(out_value, value_length, kSpeechApiKey);
#else
      return false;
#endif  // SB_HAS_SPEECH_API_KEY

    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }

  return false;
}
