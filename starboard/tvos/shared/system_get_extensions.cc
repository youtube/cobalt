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

#include "starboard/common/string.h"
#include "starboard/extension/accessibility.h"
#include "starboard/extension/configuration.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/extension/graphics.h"
#include "starboard/extension/ifa.h"
#include "starboard/extension/media/player_configuration.h"
#include "starboard/extension/media_session.h"
#include "starboard/extension/platform_service.h"
#include "starboard/system.h"
#include "starboard/tvos/shared/accessibility_extension.h"
#include "starboard/tvos/shared/configuration.h"
#include "starboard/tvos/shared/crash_handler.h"
#include "starboard/tvos/shared/graphics.h"
#include "starboard/tvos/shared/ifa.h"
#include "starboard/tvos/shared/media/player_configuration.h"
#include "starboard/tvos/shared/platform_service.h"
#include "starboard/tvos/shared/uikit_media_session_client.h"

const void* SbSystemGetExtension(const char* name) {
  if (strcmp(name, kCobaltExtensionGraphicsName) == 0) {
    return starboard::shared::uikit::GetGraphicsApi();
  }
  if (strcmp(name, kCobaltExtensionConfigurationName) == 0) {
    return starboard::shared::uikit::GetConfigurationApi();
  }
  if (strcmp(name, kCobaltExtensionMediaSessionName) == 0) {
    return starboard::shared::uikit::GetMediaSessionApi();
  }
  if (strcmp(name, kCobaltExtensionCrashHandlerName) == 0) {
    return starboard::shared::uikit::GetCrashHandlerApi();
  }
  if (strcmp(name, kCobaltExtensionPlatformServiceName) == 0) {
    return starboard::shared::uikit::GetPlatformServiceApi();
  }
  if (strcmp(name, kStarboardExtensionIfaName) == 0) {
    return starboard::shared::uikit::GetIfaApi();
  }
  if (strcmp(name, kStarboardExtensionPlayerConfigurationName) == 0) {
    return starboard::shared::uikit::GetPlayerConfigurationApi();
  }
  if (strcmp(name, kStarboardExtensionAccessibilityName) == 0) {
    return starboard::shared::uikit::GetAccessibilityApi();
  }
  return NULL;
}
