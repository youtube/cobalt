// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/extension/h5vcc_config.h"
#include "starboard/android/shared/h5vcc_config.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

// Definitions of any functions included as components in the extension
// are added here.

void EnableBackgroundPlayback(bool value) {
  JniEnvExt* env = JniEnvExt::Get();
  JniEnvExt::Get()->CallStarboardVoidMethodOrAbort("enableBackgroundPlayback",
                                                   "(Z)V", (jboolean)value);
}

const StarboardExtensionH5vccConfigApi kH5vccConfigApi = {
    kStarboardExtensionH5vccConfigName,
    1,  // API version that's implemented.
    &EnableBackgroundPlayback,
};

}  // namespace

const void* GetH5vccConfigApi() {
  return &kH5vccConfigApi;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
