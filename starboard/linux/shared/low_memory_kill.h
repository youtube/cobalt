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

#ifndef STARBOARD_LINUX_SHARED_LOW_MEMORY_KILL_H_
#define STARBOARD_LINUX_SHARED_LOW_MEMORY_KILL_H_

#include <string_view>

#include "starboard/extension/low_memory_kill.h"

namespace starboard {

// A supervisor/watchdog process that kills Cobalt because of memory pressure
// signals this to the next Cobalt session by atomically writing a marker file
// at `/tmp/cobalt_was_low_memory_killed`. The next session treats the marker's
// presence (successful `unlink()`) as "the previous session was
// low-memory-killed", parses any `key=value` lines in it for
// `GetLowMemoryKillDetails()`, and caches the result for the process lifetime.
//
// Supported optional `key=value` entries (each matching the
// `SbLowMemoryKillDetails` field of the same name):
//   timestamp_posix_usec     0..INT64_MAX (default 0)
//   foreground_status        unknown | foreground | background | 0..2
//   pressure_source          unknown | system | cobalt | 0..2
//   system_memory_available  0..INT64_MAX or -1 if unavailable (default -1)
//   system_memory_total      0..INT64_MAX or -1 if unavailable (default -1)
//   cobalt_memory_usage      0..INT64_MAX or -1 if unavailable (default -1)
//   cobalt_memory_limit      0..INT64_MAX or -1 if unavailable (default -1)
//   reason                   printable ASCII (0x20-0x7E), truncated to 127 B
const void* GetLowMemoryKillApi();

namespace testing {

// Allows tests to access the evaluation logic directly without reading or
// writing the cached value. If |out_details| is non-null, it is set to the
// details that would be reported.
bool EvaluateLowMemoryKill(SbLowMemoryKillDetails* out_details = nullptr);

// Parses marker file |contents| into an SbLowMemoryKillDetails struct, leaving
// absent or malformed fields at their unavailable defaults.
SbLowMemoryKillDetails ParseLowMemoryKillMarker(std::string_view contents);

// Resets the cached WasLowMemoryKilled() session state for testing.
void ResetLowMemoryKillStateForTesting();

}  // namespace testing

}  // namespace starboard

#endif  // STARBOARD_LINUX_SHARED_LOW_MEMORY_KILL_H_
