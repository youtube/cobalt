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

#include <string>
#include <string_view>

#include "starboard/extension/low_memory_kill.h"

namespace starboard {

// Low memory kill marker file
// ===========================
//
// A supervisor/watchdog process that kills Cobalt because of memory pressure
// signals this to the next Cobalt session by creating the marker file at
// |kLowMemoryKillMarkerPath|. The next session treats the marker's *presence*
// as "the previous session was low-memory-killed", reads its contents, and
// then deletes it. The result is cached for the lifetime of that process.
//
// The marker's contents optionally describe the kill and are reported through
// GetLowMemoryKillDetails(). The format is plain text, one "key=value" entry
// per line:
//
//   # Lines starting with '#' are comments. Blank lines are ignored.
//   foreground_status=background
//   pressure_source=system
//   system_memory_available=104857600
//   system_memory_total=2147483648
//   cobalt_memory_usage=943718400
//   cobalt_memory_limit=1073741824
//
// Keys (each maps to the SbLowMemoryKillDetails field of the same name):
//   foreground_status        One of: unknown, other, foreground, background,
//                            or the numeric SbLowMemoryKillForegroundStatus
//                            value 0-3.
//   pressure_source          One of: unknown, other, system, cobalt, or the
//                            numeric SbLowMemoryKillPressureSource value 0-3.
//   system_memory_available  Unsigned decimal integer, in bytes.
//   system_memory_total      Unsigned decimal integer, in bytes.
//   cobalt_memory_usage      Unsigned decimal integer, in bytes.
//   cobalt_memory_limit      Unsigned decimal integer, in bytes.
//
// Parsing rules:
//   - The file is plain ASCII text. Do not start it with a UTF-8 byte order
//     mark: it would make the first line unrecognized.
//   - Keys and enum names are lowercase and case-sensitive.
//   - Spaces, tabs, and a trailing '\r' around keys and values are ignored.
//   - A comment must be on its own line; text after a value is not stripped
//     and makes that value malformed.
//   - Integers are base 10 with no sign, prefix, separators, or suffix, and
//     must fit in a uint64_t (max 18446744073709551615).
//   - Every entry is optional. A field whose key is absent, or whose every
//     occurrence is malformed, is reported as 0 / Unknown. A malformed entry
//     never affects other fields.
//   - Lines that are not "key=value", unknown keys, and malformed values are
//     ignored. If a key appears more than once, the last well-formed
//     occurrence wins.
//   - Only the first 16 KiB (16384 bytes) of the file are parsed. A line cut
//     off by that limit is ignored rather than parsed partially, as is
//     everything after it. A larger marker still counts as a kill and is
//     still removed.
//   - Any content, including an empty file or the legacy "1\n", still means
//     the previous session was low-memory-killed.
//
// Write the marker atomically (write a temporary file in the same directory,
// then rename it into place) so a starting Cobalt never reads a partially
// written marker. From a shell script, e.g.:
//
//   tmp=$(mktemp /tmp/cobalt_was_low_memory_killed.XXXXXX) &&
//     printf 'pressure_source=cobalt\n' > "$tmp" &&
//     printf 'cobalt_memory_usage=%s\n' "$USAGE" >> "$tmp" &&
//     chmod 644 "$tmp" &&
//     mv "$tmp" /tmp/cobalt_was_low_memory_killed
//
// C++ watchdogs and tests can use WriteLowMemoryKillMarker() below.
//
// Cobalt must be able to delete the marker: one it cannot remove is not
// reported as a kill, and its contents are discarded. In a sticky directory
// such as /tmp, this means the marker must be owned by the user Cobalt runs
// as. A watchdog running as another user (e.g. root) must chown the temporary
// file to that user before renaming it into place.
inline constexpr char kLowMemoryKillMarkerPath[] =
    "/tmp/cobalt_was_low_memory_killed";

const void* GetLowMemoryKillApi();

// Returns |details| serialized in the marker file format described above. All
// six keys are always emitted; enum values outside the defined range are
// written as "unknown".
std::string FormatLowMemoryKillMarker(const SbLowMemoryKillDetails& details);

// Atomically writes a marker describing |details| to |path| (normally
// |kLowMemoryKillMarkerPath|) by writing a temporary file next to it and
// renaming it into place. Returns true on success.
bool WriteLowMemoryKillMarker(const char* path,
                              const SbLowMemoryKillDetails& details);

// Parses marker file |contents| as described above. Never fails: anything
// that cannot be parsed is left 0 / Unknown.
SbLowMemoryKillDetails ParseLowMemoryKillMarker(std::string_view contents);

namespace testing {

// Allows tests to access the evaluation logic directly without reading or
// writing the cached value. If |out_details| is non-null, it is set to the
// details that would be reported (all 0 / Unknown unless a marker was read).
bool EvaluateLowMemoryKill(SbLowMemoryKillDetails* out_details = nullptr);

// Resets the cached WasLowMemoryKilled() session state for testing.
void ResetLowMemoryKillStateForTesting();

}  // namespace testing

}  // namespace starboard

#endif  // STARBOARD_LINUX_SHARED_LOW_MEMORY_KILL_H_
