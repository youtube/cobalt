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

#ifndef STARBOARD_EXTENSION_CORE_METRIC_COMPONENTS_H_
#define STARBOARD_EXTENSION_CORE_METRIC_COMPONENTS_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionCoreMetricComponentsName \
  "dev.cobalt.extension.CoreMetricComponents"

typedef struct CobaltExtensionCoreMetricComponentsApi {
  // Name should be the string |kCobaltExtensionCoreMetricComponentsName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Retrieves completed crash and hang reports from local storage (e.g., the
  // Crashpad database) along with their metadata and annotations.
  //
  // Populates |out_buffer| with a null-terminated JSON-serialized array of
  // dictionaries representing the reports up to |buffer_size| bytes.
  //
  // Example JSON payload structure:
  // [
  //   {
  //     "crashpad_db_uuid": "b44678e7-3c5f-4b80-94d4-b7b18a667100",
  //     "creation_time": 1782250000.0,
  //     "cmc_report_type": "hang",
  //     "cmc_join_uuid": "fb8b78fa-c1ad-467d-afc2-4916a4a6b281"
  //   },
  //   ...
  // ]
  //
  // Mandatory keys for each dictionary element:
  // - "creation_time" (number): The UNIX timestamp when the report was
  //   captured.
  // - "cmc_report_type" (string): The type of report ("crash" or "hang").
  // - "cmc_join_uuid" (string): The Cobalt-generated session or hang ID.
  //
  // Implementations may append additional custom keys, which Cobalt
  // will ignore.
  // Returns true on success and false on failure.
  bool (*GetCompletedReports)(char* out_buffer, int buffer_size);

} CobaltExtensionCoreMetricComponentsApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_CORE_METRIC_COMPONENTS_H_
