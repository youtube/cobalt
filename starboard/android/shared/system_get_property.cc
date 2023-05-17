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

#include "sys/system_properties.h"

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/device_type.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"

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
  if (strlen(from_value) + 1 > value_length)
    return false;
  starboard::strlcpy(out_value, from_value, value_length);
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
    starboard::strlcpy(out_value, default_value, value_length);
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
      return GetAndroidSystemProperty("ro.product.brand", out_value,
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
    case kSbSystemPropertyModelYear: {
      char key1[PROP_VALUE_MAX] = "";
      GetAndroidSystemProperty("ro.oem.key1", key1, PROP_VALUE_MAX,
                               kUnknownValue);
      if (strcmp(key1, kUnknownValue) == 0 || strlen(key1) < 10) {
        return CopyStringAndTestIfSuccess(out_value, value_length,
                                          kUnknownValue);
      }
      // See
      // https://support.google.com/androidpartners_androidtv/answer/9351639?hl=en
      // for the format of key1.
      std::string year = "20";
      year += key1[9];
      year += key1[10];
      return CopyStringAndTestIfSuccess(out_value, value_length, year.c_str());
    }

    case kSbSystemPropertySystemIntegratorName:
      return GetAndroidSystemProperty("ro.product.manufacturer", out_value,
                                      value_length, kUnknownValue);

    case kSbSystemPropertyFriendlyName:
      return CopyStringAndTestIfSuccess(out_value, value_length, kFriendlyName);

    case kSbSystemPropertyPlatformName:
      return CopyAndroidPlatformName(out_value, value_length);

    case kSbSystemPropertySpeechApiKey:
      return false;
    case kSbSystemPropertyUserAgentAuxField: {
      JniEnvExt* env = JniEnvExt::Get();
      ScopedLocalJavaRef<jstring> aux_string(
          env->CallStarboardObjectMethodOrAbort("getUserAgentAuxField",
                                                "()Ljava/lang/String;"));

      std::string utf_str = env->GetStringStandardUTFOrAbort(aux_string.Get());
      bool success =
          CopyStringAndTestIfSuccess(out_value, value_length, utf_str.c_str());
      return success;
    }
#if SB_API_VERSION >= 14
    case kSbSystemPropertyAdvertisingId: {
      JniEnvExt* env = JniEnvExt::Get();
      ScopedLocalJavaRef<jstring> id_string(
          env->CallStarboardObjectMethodOrAbort("getAdvertisingId",
                                                "()Ljava/lang/String;"));
      std::string utf_str = env->GetStringStandardUTFOrAbort(id_string.Get());
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        utf_str.c_str());
    }
    case kSbSystemPropertyLimitAdTracking: {
      bool limit_ad_tracking_enabled =
          JniEnvExt::Get()->CallStarboardBooleanMethodOrAbort(
              "getLimitAdTracking", "()Z") == JNI_TRUE;
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        limit_ad_tracking_enabled ? "1" : "0");
    }
#endif
#if SB_API_VERSION >= 15
    case kSbSystemPropertyDeviceType:
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        starboard::kSystemDeviceTypeAndroidTV);
#endif
    default:
      SB_DLOG(WARNING) << __FUNCTION__
                       << ": Unrecognized property: " << property_id;
      break;
  }

  return false;
}
