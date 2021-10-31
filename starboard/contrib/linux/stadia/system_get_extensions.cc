// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
#include "cobalt/extension/configuration.h"
#include "cobalt/extension/crash_handler.h"
#include "starboard/common/string.h"
#include "starboard/contrib/stadia/get_platform_service_api.h"
#include "starboard/shared/starboard/crash_handler.h"
#include "starboard/system.h"
#if SB_IS(EVERGREEN_COMPATIBLE)
#include "starboard/elf_loader/evergreen_config.h"
#endif
#include "starboard/linux/shared/configuration.h"

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
  if (strcmp(name, kCobaltExtensionConfigurationName) == 0) {
    return starboard::shared::GetConfigurationApi();
  }
  if (strcmp(name, kCobaltExtensionCrashHandlerName) == 0) {
    return starboard::common::GetCrashHandlerApi();
  }
  if (strcmp(name, kCobaltExtensionPlatformServiceName) == 0) {
    return starboard::contrib::stadia::GetPlatformServiceApi();
  }
  return NULL;
}
