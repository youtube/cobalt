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

#ifndef STARBOARD_EXTENSION_LOADER_APP_METRICS_H_
#define STARBOARD_EXTENSION_LOADER_APP_METRICS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionLoaderAppMetricsName \
  "dev.cobalt.extension.LoaderAppMetrics"

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Must be kept in sync with the
// corresponding definition in
// tools/metrics/histograms/metadata/cobalt/enums.xml.
typedef enum CrashpadInstallationStatus {
  // The enumerators below this point were added in version 1 or later.
  kUnknown = 0,
  kSucceeded = 1,
  kFailedCrashpadHandlerBinaryNotFound = 2,
  kFailedDatabaseInitializationFailed = 3,
  kFailedSignalHandlerInstallationFailed = 4,
  kMaxValue = kFailedSignalHandlerInstallationFailed
} CrashpadInstallationStatus;

typedef struct StarboardExtensionLoaderAppMetricsApi {
  // Name should be the string |kStarboardExtensionLoaderAppMetricsName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  void (*SetCrashpadInstallationStatus)(CrashpadInstallationStatus status);

  CrashpadInstallationStatus (*GetCrashpadInstallationStatus)();

} StarboardExtensionLoaderAppMetricsApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_LOADER_APP_METRICS_H_
