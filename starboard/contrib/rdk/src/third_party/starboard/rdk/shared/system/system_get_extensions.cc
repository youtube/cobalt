// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
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

#include <cstring>


#include "starboard/common/string.h"
#include "starboard/common/log.h"
#include "starboard/extension/configuration.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/extension/loader_app_metrics.h"
#include "starboard/extension/graphics.h"
#include "third_party/starboard/rdk/shared/graphics.h"
#include "starboard/extension/platform_service.h"
#include "third_party/starboard/rdk/shared/accessibility_extension.h"
#include "third_party/starboard/rdk/shared/configuration.h"
#include "third_party/starboard/rdk/shared/platform_service.h"
#if SB_IS(EVERGREEN_COMPATIBLE)
#include "starboard/elf_loader/evergreen_config.h"
#include "starboard/shared/starboard/crash_handler.h"
#include "starboard/shared/starboard/loader_app_metrics.h"
#endif

const void* SbSystemGetExtension(const char* name) {
#if SB_IS(EVERGREEN_COMPATIBLE)
  const elf_loader::EvergreenConfig* evergreen_config =
      elf_loader::EvergreenConfig::GetInstance();
  if (evergreen_config != NULL &&
      evergreen_config->custom_get_extension_ != NULL) {
    const void* ext = evergreen_config->custom_get_extension_(name);
    if (ext != NULL) {
      return ext;
    }
  }
  if (strcmp(name, kCobaltExtensionCrashHandlerName) == 0) {
    return starboard::GetCrashHandlerApi();
  }
  if (strcmp(name, kStarboardExtensionLoaderAppMetricsName) == 0) {
    return starboard::GetLoaderAppMetricsApi();
  }
#endif
  if (strcmp(name, kCobaltExtensionConfigurationName) == 0) {
    return third_party::starboard::rdk::shared::GetConfigurationApi();
  }
  else if (strcmp(name, kCobaltExtensionGraphicsName) == 0) {
    return starboard::rdk::shared::GetGraphicsApi();
  }
  else if (strcmp(name, kCobaltExtensionPlatformServiceName) == 0) {
    return third_party::starboard::rdk::shared::GetPlatformServiceApi();
  }
  if (strcmp(name, kStarboardExtensionAccessibilityName) == 0) {
    return third_party::starboard::rdk::shared::GetAccessibilityApi();
  }
  return NULL;
}

