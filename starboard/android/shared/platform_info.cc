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

#include "base/android/jni_android.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/extension/platform_info.h"

namespace starboard::android::shared {

namespace {

// TODO: b/372559388 - Update namespace to jni_zero.
using base::android::AttachCurrentThread;

bool GetFirmwareVersionDetails(char* out_value, int value_length) {
  JNIEnv* env = AttachCurrentThread();
  std::string utf_str =
      StarboardBridge::GetInstance()->GetBuildFingerprint(env);
  if (utf_str.length() + 1 > static_cast<size_t>(value_length)) {
    return false;
  }
  starboard::strlcpy(out_value, utf_str.c_str(), value_length);
  return true;
}

const char* GetOsExperience() {
  JNIEnv* env = AttachCurrentThread();
  return StarboardBridge::GetInstance()->IsAmatiDevice(env) ? "Amati"
                                                            : "Watson";
}

int64_t GetCoreServicesVersion() {
  JNIEnv* env = AttachCurrentThread();
  return StarboardBridge::GetInstance()->GetPlayServicesVersion(env);
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
