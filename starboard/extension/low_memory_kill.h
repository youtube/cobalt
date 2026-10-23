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

#ifndef STARBOARD_EXTENSION_LOW_MEMORY_KILL_H_
#define STARBOARD_EXTENSION_LOW_MEMORY_KILL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionLowMemoryKillName \
  "dev.starboard.extension.LowMemoryKill"

typedef enum SbLowMemoryKillForegroundStatus {
  kSbLowMemoryKillForegroundStatusUnknown = 0,
  kSbLowMemoryKillForegroundStatusOther = 1,
  kSbLowMemoryKillForegroundStatusForeground = 2,
  kSbLowMemoryKillForegroundStatusBackground = 3,
} SbLowMemoryKillForegroundStatus;

typedef enum SbLowMemoryKillPressureSource {
  kSbLowMemoryKillPressureSourceUnknown = 0,
  kSbLowMemoryKillPressureSourceOther = 1,
  // The OS needed to reclaim memory for tasks other than Cobalt.
  kSbLowMemoryKillPressureSourceSystem = 2,
  // Cobalt's memory usage reached or exceeded its allowed limit.
  kSbLowMemoryKillPressureSourceCobalt = 3,
} SbLowMemoryKillPressureSource;

// All fields are optional. A value of 0 indicates information is not available.
// Memory values are in bytes, as measured at (or as close as possible to) the
// time of the kill.
// The layout of this struct is part of version 2 of this API and must not be
// modified in place.
typedef struct SbLowMemoryKillDetails {
  SbLowMemoryKillForegroundStatus foreground_status;
  SbLowMemoryKillPressureSource pressure_source;
  uint64_t system_memory_available;
  uint64_t system_memory_total;
  uint64_t cobalt_memory_usage;
  uint64_t cobalt_memory_limit;
} SbLowMemoryKillDetails;

typedef struct StarboardExtensionLowMemoryKillApi {
  // Name should be the string |kStarboardExtensionLowMemoryKillName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Returns true if the previous Cobalt session was killed by the system due to
  // memory pressure. Must return the same value across calls for the lifetime
  // of the process.
  bool (*WasLowMemoryKilled)();

  // The fields below this point were added in version 2 or later.

  // If `WasLowMemoryKilled()` would return true and `out_details` is non-null,
  // populates `out_details` with information about why the previous session was
  // killed and returns true. Otherwise leaves `*out_details` unmodified and
  // returns false. Must return the same result across calls for the lifetime of
  // the process.
  bool (*GetLowMemoryKillDetails)(SbLowMemoryKillDetails* out_details);
} StarboardExtensionLowMemoryKillApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_LOW_MEMORY_KILL_H_
