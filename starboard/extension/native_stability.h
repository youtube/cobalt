// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_EXTENSION_NATIVE_STABILITY_H_
#define STARBOARD_EXTENSION_NATIVE_STABILITY_H_

#include <stdint.h>

#include "starboard/configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionNativeStabilityName \
  "dev.starboard.extension.NativeStability"

typedef enum SbNativeStabilityReportType {
  kSbNativeStabilityReportUnknown = 0,
  kSbNativeStabilityReportCrash = 1,
  kSbNativeStabilityReportHang = 2,
} SbNativeStabilityReportType;

typedef struct SbNativeStabilityReport {
  // A null-terminated string holding the 36-character canonical
  // representation of a UUID (formatted as 8-4-4-4-12 hex characters with
  // hyphens) plus 1 byte for the null terminator.
  char native_stability_event_uuid[37];
  int64_t event_time_s;
  SbNativeStabilityReportType report_type;
} SbNativeStabilityReport;

typedef int (*ReadReportsCallback)(SbNativeStabilityReport* reports,
                                   int max_num_reports);

typedef struct StarboardExtensionNativeStabilityApi {
  // Name should be the string |kStarboardExtensionNativeStabilityName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Reads crash and hang reports from local storage (e.g., the Crashpad
  // database) along with their metadata and annotations.
  //
  // Populates |reports| with up to |max_num_reports| stability reports.
  // Returns the number of reports populated, or -1 on failure.
  int (*ReadReports)(SbNativeStabilityReport* reports, int max_num_reports);

  // Registers the provided callback to be called by the extension's
  // |ReadReports| implementation. This is used to delegate to an injected
  // dependency to avoid dependency cycles.
  void (*RegisterReadReportsCallback)(ReadReportsCallback callback);
} StarboardExtensionNativeStabilityApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_NATIVE_STABILITY_H_
