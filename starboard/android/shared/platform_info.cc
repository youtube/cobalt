// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/platform_info.h"

#include <string>

#include "starboard/common/log.h"

#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/string.h"
#include "starboard/extension/platform_info.h"

namespace starboard::android::shared {

namespace {

bool GetFirmwareVersionDetails(char* out_value, int value_length) {
  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jstring> id_string(env->CallStarboardObjectMethodOrAbort(
      "getBuildFingerprint", "()Ljava/lang/String;"));
  std::string utf_str = env->GetStringStandardUTFOrAbort(id_string.Get());
  if (strlen(utf_str.c_str()) + 1 > value_length) {
    return false;
  }
  starboard::strlcpy(out_value, utf_str.c_str(), value_length);
  return true;
}

const char* GetOsExperience() {
  bool is_amati = JniEnvExt::Get()->CallStarboardBooleanMethodOrAbort(
      "getIsAmatiDevice", "()Z");
  if (is_amati) {
    return "Amati";
  }
  return "Watson";
}

int64_t GetCoreServicesVersion() {
  return JniEnvExt::Get()->CallStarboardLongMethodOrAbort(
      "getPlayServicesVersion", "()J");
}

// clang-format off
const CobaltExtensionPlatformInfoApi kPlatformInfoApi = {
    kCobaltExtensionPlatformInfoName,
    2,
    &GetFirmwareVersionDetails,
    &GetOsExperience,
    &GetCoreServicesVersion,
};
// clang-format on

}  // namespace

const void* GetPlatformInfoApi() {
  return &kPlatformInfoApi;
}

}  // namespace starboard::android::shared
