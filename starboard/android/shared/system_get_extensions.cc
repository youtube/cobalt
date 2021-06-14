// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/extension/configuration.h"
#include "cobalt/extension/media_session.h"
#include "cobalt/extension/platform_service.h"
#include "starboard/android/shared/android_media_session_client.h"
#include "starboard/android/shared/configuration.h"
#include "starboard/android/shared/platform_service.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"

const void* SbSystemGetExtension(const char* name) {
  if (strcmp(name, kCobaltExtensionPlatformServiceName) == 0) {
    return starboard::android::shared::GetPlatformServiceApi();
  }
  if (strcmp(name, kCobaltExtensionConfigurationName) == 0) {
    return starboard::android::shared::GetConfigurationApi();
  }
  if (strcmp(name, kCobaltExtensionMediaSessionName) == 0) {
    return starboard::android::shared::GetMediaSessionApi();
  }
  return NULL;
}
