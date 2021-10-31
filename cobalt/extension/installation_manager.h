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

#ifndef COBALT_EXTENSION_INSTALLATION_MANAGER_H_
#define COBALT_EXTENSION_INSTALLATION_MANAGER_H_

#include <stdint.h>

#include "starboard/configuration.h"

#define IM_EXT_MAX_APP_KEY_LENGTH 1024
#define IM_EXT_INVALID_INDEX -1
#define IM_EXT_ERROR -1
#define IM_EXT_SUCCESS 0

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionInstallationManagerName \
  "dev.cobalt.extension.InstallationManager"

typedef struct CobaltExtensionInstallationManagerApi {
  // Name should be the string kCobaltExtensionInstallationManagerName.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // Installation Manager API wrapper.
  // For more details, check:
  //  starboard/loader_app/installation_manager.h

  int (*GetCurrentInstallationIndex)();
  int (*MarkInstallationSuccessful)(int installation_index);
  int (*RequestRollForwardToInstallation)(int installation_index);
  int (*GetInstallationPath)(int installation_index, char* path,
                             int path_length);
  int (*SelectNewInstallationIndex)();
  int (*GetAppKey)(char* app_key, int app_key_length);
  int (*GetMaxNumberInstallations)();
  int (*ResetInstallation)(int installation_index);
  int (*Reset)();
} CobaltExtensionInstallationManagerApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_INSTALLATION_MANAGER_H_
