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

#include <stdbool.h>
#include <stdint.h>

#include "starboard/extension/macro.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum size in bytes of `SbLowMemoryKillDetails::reason`, including the
// trailing null terminator.
#define kSbLowMemoryKillReasonMaxSize 128

typedef enum SbLowMemoryKillForegroundStatus {
  kSbLowMemoryKillForegroundStatusUnknown = 0,
  kSbLowMemoryKillForegroundStatusForeground = 1,
  kSbLowMemoryKillForegroundStatusBackground = 2,
} SbLowMemoryKillForegroundStatus;

typedef enum SbLowMemoryKillPressureSource {
  kSbLowMemoryKillPressureSourceUnknown = 0,
  // The OS needed to reclaim memory for tasks other than Cobalt.
  kSbLowMemoryKillPressureSourceSystem = 1,
  // Cobalt's memory usage reached or exceeded its allowed limit.
  kSbLowMemoryKillPressureSourceCobalt = 2,
} SbLowMemoryKillPressureSource;

// `timestamp_posix_usec` is required. All other fields are optional.
// For optional int properties, a value of -1 indicates information is not
// available (0 indicates 0 bytes). For enum properties, 0 (`Unknown`)
// indicates information is not available. For `reason`, an empty string
// (`reason[0] == '\0'`) indicates information is not available.
// Memory values are in bytes, as measured at (or as close as possible to) the
// time of the kill.
// The layout of this struct is part of version 2 of this API and must not be
// modified in place.
typedef struct SbLowMemoryKillDetails {
  // Wall-clock time of the kill in microseconds since the POSIX epoch
  // (1970-01-01T00:00:00Z).
  int64_t timestamp_posix_usec;
  SbLowMemoryKillForegroundStatus foreground_status;
  SbLowMemoryKillPressureSource pressure_source;
  int64_t system_memory_available;
  int64_t system_memory_total;
  int64_t cobalt_memory_usage;
  int64_t cobalt_memory_limit;
  // Null-terminated UTF-8 string describing why the session was killed, or
  // empty (`reason[0] == '\0'`) if not available. Truncated to fit if longer
  // than `kSbLowMemoryKillReasonMaxSize - 1` bytes.
  char reason[kSbLowMemoryKillReasonMaxSize];
} SbLowMemoryKillDetails;

SB_EXTENSION(
    LowMemoryKill,
    // The fields below this point were added in version 1 or later.

    // Returns true if the previous Cobalt session was killed by the system due
    // to memory pressure. Must return the same value across calls for the
    // lifetime of the process.
    bool (*WasLowMemoryKilled)();

    // The fields below this point were added in version 2 or later.

    // If `WasLowMemoryKilled()` would return true and `out_details` is
    // non-null, populates `out_details` with information about why the previous
    // session was killed and returns true. Otherwise leaves `*out_details`
    // unmodified and returns false. Must return the same result across calls
    // for the lifetime of the process.
    bool (*GetLowMemoryKillDetails)(SbLowMemoryKillDetails* out_details););

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_LOW_MEMORY_KILL_H_
