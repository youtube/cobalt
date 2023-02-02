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

#include "starboard/android/shared/android_media_session_client.h"
#include "starboard/android/shared/configuration.h"
#include "starboard/android/shared/crash_handler.h"
#include "starboard/android/shared/graphics.h"
#include "starboard/android/shared/platform_service.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#if SB_IS(EVERGREEN_COMPATIBLE)
#include "starboard/elf_loader/evergreen_config.h"  // nogncheck
#endif
#include "starboard/extension/configuration.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/extension/graphics.h"
#include "starboard/extension/media_session.h"
#include "starboard/extension/platform_service.h"

const void* SbSystemGetExtension(const char* name) {
#if SB_IS(EVERGREEN_COMPATIBLE)
  const starboard::elf_loader::EvergreenConfig* evergreen_config =
      starboard::elf_loader::EvergreenConfig::GetInstance();
  if (evergreen_config != NULL &&
      evergreen_config->custom_get_extension_ != NULL) {
    const void* ext = evergreen_config->custom_get_extension_(name);
    if (ext != NULL) {
      return ext;
    }
  }
#endif
  if (strcmp(name, kCobaltExtensionPlatformServiceName) == 0) {
    return starboard::android::shared::GetPlatformServiceApi();
  }
  if (strcmp(name, kCobaltExtensionConfigurationName) == 0) {
    return starboard::android::shared::GetConfigurationApi();
  }
  if (strcmp(name, kCobaltExtensionMediaSessionName) == 0) {
    return starboard::android::shared::GetMediaSessionApi();
  }
  if (strcmp(name, kCobaltExtensionGraphicsName) == 0) {
    return starboard::android::shared::GetGraphicsApi();
  }
  if (strcmp(name, kCobaltExtensionCrashHandlerName) == 0) {
    return starboard::android::shared::GetCrashHandlerApi();
  }
  return NULL;
}
