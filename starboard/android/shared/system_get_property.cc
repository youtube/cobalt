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

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/log.h"
#include "starboard/string.h"

// We can't #include "base/stringize_macros.h" in Starboard
#define STRINGIZE_NO_EXPANSION(x) #x
#define STRINGIZE(x) STRINGIZE_NO_EXPANSION(x)

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;

namespace {

const char kFriendlyName[] = "Android";
const char kUnknownValue[] = "unknown";
// This is a format string template and the %s is meant to be replaced by
// the Android release version number (e.g. "7.0" for Nougat).
const char kPlatformNameFormat[] =
    "Linux " STRINGIZE(ANDROID_ABI) "; Android %s";

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

  if (SbStringGetLength(out_value) == 0) {
    SbStringCopy(out_value, default_value, value_length);
  }
  return true;
}

// Populate the kPlatformNameFormat with the version number and if
// |value_length| is large enough to store the result, copy the result into
// |out_value|.
bool CopyAndroidPlatformName(char* out_value, int value_length) {
  // Get the Android version number (e.g. "7.0" for Nougat).
  const int kStringBufferSize = 256;
  char version_string_buffer[kStringBufferSize];
  if (!GetAndroidSystemProperty("ro.build.version.release",
                                version_string_buffer, kStringBufferSize,
                                kUnknownValue)) {
    return false;
  }

  char result_string[kStringBufferSize];
  SbStringFormatF(result_string, kStringBufferSize, kPlatformNameFormat,
                  version_string_buffer);

  return CopyStringAndTestIfSuccess(out_value, value_length, result_string);
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
      return CopyAndroidPlatformName(out_value, value_length);

    case kSbSystemPropertyPlatformUuid:
      SB_NOTIMPLEMENTED();
      return CopyStringAndTestIfSuccess(out_value, value_length, "N/A");

    case kSbSystemPropertySpeechApiKey:
      return false;
    case kSbSystemPropertyUserAgentAuxField: {
      JniEnvExt* env = JniEnvExt::Get();
      ScopedLocalJavaRef<jstring> aux_string(
          env->CallStarboardObjectMethodOrAbort(
              "getUserAgentAuxField", "()Ljava/lang/String;"));

      std::string utf_str = env->GetStringStandardUTFOrAbort(aux_string.Get());
      bool success =
          CopyStringAndTestIfSuccess(out_value, value_length, utf_str.c_str());
      return success;
    }
    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }

  return false;
}
