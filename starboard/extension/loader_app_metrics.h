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
typedef enum class CrashpadInstallationStatus {
  // The enumerators below this point were added in version 1 or later.
  kUnknown = 0,
  kSucceeded = 1,
  kFailedCrashpadHandlerBinaryNotFound = 2,
  kFailedDatabaseInitializationFailed = 3,
  kFailedSignalHandlerInstallationFailed = 4,
  kMaxValue = kFailedSignalHandlerInstallationFailed
} CrashpadInstallationStatus;

typedef enum class SlotSelectionStatus {
  // The enumerators below this point were added in version 3 or later.
  kUnknown = 0,
  kCurrentSlot = 1,
  kRollForward = 2,
  kRollBackOutOfRetries = 3,
  kRollBackNoLibFile = 4,
  kRollBackBadAppKeyFile = 5,
  kRollBackSlotDraining = 6,
  kRollBackFailedToAdopt = 7,
  kRollBackFailedToLoadCobalt = 8,
  kRollBackFailedToCheckSabi = 9,
  kRollBackFailedToLookUpSymbols = 10,
  kEGLite = 11,
  kLoadSysImgFailedToInitInstallationManager = 12,
  kMaxValue = kLoadSysImgFailedToInitInstallationManager,
} SlotSelectionStatus;

typedef struct StarboardExtensionLoaderAppMetricsApi {
  // Name should be the string |kStarboardExtensionLoaderAppMetricsName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The accessors and mutators below are assumed to be called from the same
  // thread: Cobalt's application thread.

  // The fields below this point were added in version 1 or later.

  void (*SetCrashpadInstallationStatus)(CrashpadInstallationStatus status);
  CrashpadInstallationStatus (*GetCrashpadInstallationStatus)();

  // The fields below this point were added in version 2 or later.

  void (*SetElfLibraryStoredCompressed)(bool compressed);
  bool (*GetElfLibraryStoredCompressed)();

  // Records the number of microseconds it took for the ELF dynamic shared
  // library to be loaded. If the library is stored as a compressed file then
  // the decompression duration (see mutator and accessor below) is considered
  // to be contained within this ELF load duration.
  void (*SetElfLoadDurationMicroseconds)(int64_t microseconds);

  // Returns the number of microseconds it took for the ELF dynamic shared
  // library to be loaded, or -1 if this value was not set.
  int64_t (*GetElfLoadDurationMicroseconds)();

  // Records the number of microseconds it took for the ELF dynamic shared
  // library, stored as a compressed file, to be decompressed.
  void (*SetElfDecompressionDurationMicroseconds)(int64_t microseconds);

  // Returns the number of microseconds it took for the ELF dynamic shared
  // library, stored as a compressed file, to be decompressed, or -1 if this
  // value was not set.
  int64_t (*GetElfDecompressionDurationMicroseconds)();

  // Updates the extension such that GetMaxSampledUsedCpuBytesDuringElfLoad()
  // will return |bytes| iff |bytes| is greater than the value
  // GetMaxSampledUsedCpuBytesDuringElfLoad() was previously set to return.
  void (*RecordUsedCpuBytesDuringElfLoad)(int64_t bytes);

  // Returns the greatest value of used CPU bytes that was recorded by a caller
  // using RecordUsedCpuBytesDuringElfLoad(), or -1 if no value was recorded.
  int64_t (*GetMaxSampledUsedCpuBytesDuringElfLoad)();

  // The fields below this point were added in version 3 or later.

  void (*SetSlotSelectionStatus)(SlotSelectionStatus status);
  SlotSelectionStatus (*GetSlotSelectionStatus)();

} StarboardExtensionLoaderAppMetricsApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_LOADER_APP_METRICS_H_
