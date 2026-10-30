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

#include "starboard/linux/shared/low_memory_kill.h"

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>

#include "starboard/common/log.h"
#include "starboard/extension/low_memory_kill.h"
#include "starboard/shared/environment.h"

namespace starboard {

namespace {

constexpr char kTestEnvVar[] = "COBALT_WAS_LOW_MEMORY_KILLED";
constexpr char kTmpMarkerPath[] = "/tmp/cobalt_was_low_memory_killed";

// Marker file keys. See low_memory_kill.h for the format.
constexpr std::string_view kTimestampPosixUsecKey = "timestamp_posix_usec";
constexpr std::string_view kForegroundStatusKey = "foreground_status";
constexpr std::string_view kPressureSourceKey = "pressure_source";
constexpr std::string_view kSystemMemoryAvailableKey =
    "system_memory_available";
constexpr std::string_view kSystemMemoryTotalKey = "system_memory_total";
constexpr std::string_view kCobaltMemoryUsageKey = "cobalt_memory_usage";
constexpr std::string_view kCobaltMemoryLimitKey = "cobalt_memory_limit";
constexpr std::string_view kReasonKey = "reason";

// Enum value names, indexed by enum value.
constexpr std::string_view kForegroundStatusNames[] = {
    "unknown",
    "foreground",
    "background",
};
static_assert(kSbLowMemoryKillForegroundStatusUnknown == 0 &&
                  kSbLowMemoryKillForegroundStatusForeground == 1 &&
                  kSbLowMemoryKillForegroundStatusBackground == 2,
              "kForegroundStatusNames must be indexed by enum value");

constexpr std::string_view kPressureSourceNames[] = {
    "unknown",
    "system",
    "cobalt",
};
static_assert(kSbLowMemoryKillPressureSourceUnknown == 0 &&
                  kSbLowMemoryKillPressureSourceSystem == 1 &&
                  kSbLowMemoryKillPressureSourceCobalt == 2,
              "kPressureSourceNames must be indexed by enum value");

constexpr SbLowMemoryKillDetails MakeUnavailableDetails() {
  return SbLowMemoryKillDetails{
      /*timestamp_posix_usec=*/0,
      kSbLowMemoryKillForegroundStatusUnknown,
      kSbLowMemoryKillPressureSourceUnknown,
      /*system_memory_available=*/-1,
      /*system_memory_total=*/-1,
      /*cobalt_memory_usage=*/-1,
      /*cobalt_memory_limit=*/-1,
      /*reason=*/"",
  };
}

std::string_view Trim(std::string_view s) {
  constexpr char kWhitespace[] = " \t\r";
  size_t begin = s.find_first_not_of(kWhitespace);
  if (begin == std::string_view::npos) {
    return {};
  }
  size_t end = s.find_last_not_of(kWhitespace);
  return s.substr(begin, end - begin + 1);
}

// Parses a non-negative base-10 integer in [0, INT64_MAX] that spans all of
// |s|. Rejects signs (including "-0" and "+"), prefixes, whitespace, trailing
// characters, and values > INT64_MAX.
bool ParseNonNegativeInt64(std::string_view s, int64_t* out) {
  if (s.empty() || s.front() == '-') {
    return false;
  }
  int64_t value = 0;
  const char* end = s.data() + s.size();
  auto [ptr, ec] = std::from_chars(s.data(), end, value);
  if (ec != std::errc() || ptr != end) {
    return false;
  }
  *out = value;
  return true;
}

// Parses a memory field value: either "-1" (unavailable) or a non-negative
// base-10 byte count in [0, INT64_MAX].
bool ParseMemoryBytes(std::string_view s, int64_t* out) {
  if (s == "-1") {
    *out = -1;
    return true;
  }
  return ParseNonNegativeInt64(s, out);
}

// Parses |s| as either an enum value name from |names| or its numeric value.
template <typename EnumType, size_t N>
bool ParseEnum(std::string_view s,
               const std::string_view (&names)[N],
               EnumType* out) {
  for (size_t i = 0; i < N; ++i) {
    if (s == names[i]) {
      *out = static_cast<EnumType>(i);
      return true;
    }
  }
  int64_t value = 0;
  if (ParseNonNegativeInt64(s, &value) && value < static_cast<int64_t>(N)) {
    *out = static_cast<EnumType>(value);
    return true;
  }
  return false;
}

constexpr bool IsPrintableAscii(std::string_view s) {
  for (char ch : s) {
    auto byte = static_cast<unsigned char>(ch);
    if (byte < 0x20 || byte > 0x7e) {
      return false;
    }
  }
  return true;
}

// Parses |s| into |out_reason|, truncating to |kSbLowMemoryKillReasonMaxSize -
// 1| bytes and null-terminating. Rejects strings containing non-printable-ASCII
// bytes, leaving |out_reason| unchanged.
bool ParseReason(std::string_view s,
                 char (&out_reason)[kSbLowMemoryKillReasonMaxSize]) {
  if (!IsPrintableAscii(s)) {
    return false;
  }
  const size_t copy_len =
      std::min(s.size(), size_t{kSbLowMemoryKillReasonMaxSize - 1});
  if (copy_len > 0) {
    memcpy(out_reason, s.data(), copy_len);
  }
  out_reason[copy_len] = '\0';
  return true;
}

// Parses a single "key=value" line into |details|. Returns false if the line
// was not recognized or its value was malformed, leaving |details| unchanged.
bool ParseMarkerLine(std::string_view line, SbLowMemoryKillDetails* details) {
  size_t eq = line.find('=');
  if (eq == std::string_view::npos) {
    return false;
  }
  std::string_view key = Trim(line.substr(0, eq));
  std::string_view value = Trim(line.substr(eq + 1));

  if (key == kTimestampPosixUsecKey) {
    return ParseNonNegativeInt64(value, &details->timestamp_posix_usec);
  }
  if (key == kForegroundStatusKey) {
    return ParseEnum(value, kForegroundStatusNames,
                     &details->foreground_status);
  }
  if (key == kPressureSourceKey) {
    return ParseEnum(value, kPressureSourceNames, &details->pressure_source);
  }
  if (key == kSystemMemoryAvailableKey) {
    return ParseMemoryBytes(value, &details->system_memory_available);
  }
  if (key == kSystemMemoryTotalKey) {
    return ParseMemoryBytes(value, &details->system_memory_total);
  }
  if (key == kCobaltMemoryUsageKey) {
    return ParseMemoryBytes(value, &details->cobalt_memory_usage);
  }
  if (key == kCobaltMemoryLimitKey) {
    return ParseMemoryBytes(value, &details->cobalt_memory_limit);
  }
  if (key == kReasonKey) {
    return ParseReason(value, details->reason);
  }
  return false;
}

// Reads and then removes the marker at |path|. The marker's presence (i.e. a
// successful unlink) is what signals a low memory kill; its contents only
// supply |out_details|.
bool CheckAndConsumeMarker(const char* path,
                           SbLowMemoryKillDetails* out_details) {
  std::ifstream file(path, std::ios::in | std::ios::binary);
  std::string contents;
  if (file) {
    contents.assign(std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>());
  }
  if (unlink(path) != 0) {
    return false;
  }
  *out_details = testing::ParseLowMemoryKillMarker(contents);
  return true;
}

// Implements a two-phase check for possible LMK signals.
//   1. Test Environment Variable Override - allows for easy testing of the LMK
//      API. It carries no details.
//   2. Supervisor/Watchdog Marker File - checks for a breadcrumb file left in
//      /tmp by a memory watchdog process, reads the kill details from it, then
//      deletes the file.
// Note that (1) does not clear any marker files that might exist.
// |out_details| is always reset, and is only populated from a marker.
// The caller must hold a lock on GetMutex() to prevent races for
// CheckAndConsumeMarker().
bool EvaluateWasLowMemoryKilled(SbLowMemoryKillDetails* out_details) {
  *out_details = MakeUnavailableDetails();
  std::string env_val = starboard::GetEnvironment(kTestEnvVar);
  if (!env_val.empty()) {
    if (env_val == "1" || env_val == "true") {
      return true;
    }
    if (env_val == "0" || env_val == "false") {
      return false;
    }
    SB_LOG(WARNING)
        << "Ignoring unrecognized COBALT_WAS_LOW_MEMORY_KILLED value: \""
        << env_val << "\".";
  }

  return CheckAndConsumeMarker(kTmpMarkerPath, out_details);
}

// Since an LMK signal may be cleaned up after it is first read, we store the
// results of the first evaluation here and retain them for the life of the
// current process.
// These global variables are protected by the mutex in GetMutex().
static bool g_evaluated = false;
static bool g_was_killed = false;
static SbLowMemoryKillDetails g_details = MakeUnavailableDetails();

std::mutex& GetMutex() {
  static std::mutex mutex;
  return mutex;
}

// The caller must hold a lock on GetMutex().
void EnsureEvaluatedLocked() {
  if (!g_evaluated) {
    g_was_killed = EvaluateWasLowMemoryKilled(&g_details);
    g_evaluated = true;
  }
}

bool WasLowMemoryKilled() {
  auto lock = std::lock_guard(GetMutex());
  EnsureEvaluatedLocked();
  return g_was_killed;
}

bool GetLowMemoryKillDetails(SbLowMemoryKillDetails* out_details) {
  if (!out_details) {
    return false;
  }
  auto lock = std::lock_guard(GetMutex());
  EnsureEvaluatedLocked();
  if (!g_was_killed) {
    return false;
  }
  *out_details = g_details;
  return true;
}

const StarboardExtensionLowMemoryKillApi kLowMemoryKillApi = {
    kStarboardExtensionLowMemoryKillName,
    2,  // API version that's implemented.
    &WasLowMemoryKilled,
    &GetLowMemoryKillDetails,
};

}  // namespace

const void* GetLowMemoryKillApi() {
  return &kLowMemoryKillApi;
}

namespace testing {

SbLowMemoryKillDetails ParseLowMemoryKillMarker(std::string_view contents) {
  SbLowMemoryKillDetails details = MakeUnavailableDetails();
  int ignored_lines = 0;
  size_t pos = 0;
  while (pos < contents.size()) {
    size_t eol = contents.find('\n', pos);
    if (eol == std::string_view::npos) {
      eol = contents.size();
    }
    std::string_view line = Trim(contents.substr(pos, eol - pos));
    pos = eol + 1;
    // Skip blank lines, comments, and the legacy "1" marker contents.
    if (line.empty() || line.front() == '#' || line == "1") {
      continue;
    }
    if (!ParseMarkerLine(line, &details)) {
      ++ignored_lines;
    }
  }
  if (ignored_lines > 0) {
    SB_LOG(INFO) << "Ignored " << ignored_lines
                 << " unrecognized or malformed line(s) in the low memory "
                    "kill marker.";
  }
  return details;
}

bool EvaluateLowMemoryKill(SbLowMemoryKillDetails* out_details) {
  auto lock = std::lock_guard(GetMutex());
  SbLowMemoryKillDetails details = MakeUnavailableDetails();
  bool was_killed = EvaluateWasLowMemoryKilled(&details);
  if (out_details) {
    *out_details = details;
  }
  return was_killed;
}

void ResetLowMemoryKillStateForTesting() {
  auto lock = std::lock_guard(GetMutex());
  g_evaluated = false;
  g_was_killed = false;
  g_details = MakeUnavailableDetails();
}

}  // namespace testing

}  // namespace starboard
