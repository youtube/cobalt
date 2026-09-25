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

// clang-format off
#include "starboard/system.h"
// clang-format on

#include "build/build_config.h"
#include "starboard/android/shared/accessibility_extension.h"
#include "starboard/android/shared/android_media_session_client.h"
#include "starboard/android/shared/configuration.h"
#include "starboard/android/shared/crash_handler.h"
#include "starboard/android/shared/features_extension.h"
#include "starboard/android/shared/graphics.h"
#include "starboard/android/shared/platform_info.h"
#include "starboard/android/shared/platform_service.h"
#include "starboard/android/shared/player_settings.h"
#include "starboard/android/shared/system_info_api.h"
#include "starboard/common/string.h"
#include "starboard/elf_loader/evergreen_config.h"
#include "starboard/extension/configuration.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "starboard/extension/features.h"
#include "starboard/extension/graphics.h"
#include "starboard/extension/media_session.h"
#include "starboard/extension/platform_info.h"
#include "starboard/extension/platform_service.h"
#include "starboard/extension/player_settings.h"
#include "starboard/extension/system_info.h"
#include "starboard/shared/starboard/experimental_features.h"

const void* SbSystemGetExtension(const char* name) {
#if BUILDFLAG(IS_STARBOARD)
  // Extensions the loader app injects into the Evergreen binary, such as the
  // installation manager, take precedence over the platform's own.
  const elf_loader::EvergreenConfig* evergreen_config =
      elf_loader::EvergreenConfig::GetInstance();
  if (evergreen_config != nullptr &&
      evergreen_config->custom_get_extension_ != nullptr) {
    const void* ext = evergreen_config->custom_get_extension_(name);
    if (ext != nullptr) {
      return ext;
    }
  }
#endif
  if (strcmp(name, kCobaltExtensionPlatformServiceName) == 0) {
    return starboard::GetPlatformServiceApiAndroid();
  }
  if (strcmp(name, kCobaltExtensionConfigurationName) == 0) {
    return starboard::GetConfigurationApiAndroid();
  }
  if (strcmp(name, kCobaltExtensionMediaSessionName) == 0) {
    // TODO(b/377019873): Re-enable
    // return starboard::GetMediaSessionApi();
    return NULL;
  }
  if (strcmp(name, kStarboardExtensionFeaturesName) == 0) {
    return starboard::GetFeaturesApi();
  }
  if (strcmp(name, kCobaltExtensionGraphicsName) == 0) {
    // TODO(b/377052944): Check if this is needed, likely can be
    // deleted.
    // return starboard::GetGraphicsApi();
    return NULL;
  }
  if (strcmp(name, kCobaltExtensionCrashHandlerName) == 0) {
    return starboard::GetCrashHandlerApi();
  }
  if (strcmp(name, kCobaltExtensionPlatformInfoName) == 0) {
    return starboard::GetPlatformInfoApi();
  }
  if (strcmp(name, kStarboardExtensionExperimentalFeaturesConfigurationName) ==
      0) {
    return starboard::GetExperimentalFeaturesConfigurationApi();
  }
  if (strcmp(name, kStarboardExtensionPlayerSettingsName) == 0) {
    return starboard::GetPlayerSettingsApi();
  }
  if (strcmp(name, kStarboardExtensionAccessibilityName) == 0) {
    // TODO(b/377052218): Re-enable
    // return starboard::GetAccessibilityApi();
    return NULL;
  }
  if (strcmp(name, kStarboardExtensionSystemInfoName) == 0) {
    return starboard::GetSystemInfoApi();
  }
  return NULL;
}
