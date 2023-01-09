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

#ifndef STARBOARD_EXTENSION_UPDATER_NOTIFICATION_H_
#define STARBOARD_EXTENSION_UPDATER_NOTIFICATION_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionUpdaterNotificationName \
  "dev.cobalt.extension.UpdaterNotification"

typedef enum CobaltExtensionUpdaterNotificationState {
  kCobaltExtensionUpdaterNotificationStateNone = 0,
  kCobaltExtensionUpdaterNotificationStateChecking = 1,
  kCobaltExtensionUpdaterNotificationStateUpdateAvailable = 2,
  kCobaltExtensionUpdaterNotificationStateDownloading = 3,
  kCobaltExtensionUpdaterNotificationStateDownloaded = 4,
  kCobaltExtensionUpdaterNotificationStateInstalling = 5,
#if SB_API_VERSION > 13
  kCobaltExtensionUpdaterNotificationStateUpdated = 6,
  kCobaltExtensionUpdaterNotificationStateUpToDate = 7,
  kCobaltExtensionUpdaterNotificationStateUpdateFailed = 8,
#else
  kCobaltExtensionUpdaterNotificationStatekUpdated = 6,
  kCobaltExtensionUpdaterNotificationStatekUpToDate = 7,
  kCobaltExtensionUpdaterNotificationStatekUpdateFailed = 8,
#endif
} CobaltExtensionUpdaterNotificationState;

typedef struct CobaltExtensionUpdaterNotificationApi {
  // Name should be the string |kCobaltExtensionUpdaterNotificationName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Notify the Starboard implementation of the updater state.
  // The Starboard platform can check if the device is low on storage
  // and prompt the user to free some storage. The implementation
  // should keep track of the frequency of showing the prompt to the
  // user and try to minimize the number of user notifications.
  void (*UpdaterState)(CobaltExtensionUpdaterNotificationState state,
                       const char* current_evergreen_version);
} CobaltExtensionUpdaterNotificationApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_UPDATER_NOTIFICATION_H_
