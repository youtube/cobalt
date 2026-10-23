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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <charconv>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

#include "starboard/common/log.h"
#include "starboard/extension/low_memory_kill.h"
#include "starboard/shared/environment.h"

namespace starboard {

namespace {

constexpr char kTestEnvVar[] = "COBALT_WAS_LOW_MEMORY_KILLED";

// Upper bound on how much of the marker file is examined. A well-formed
// marker is a few hundred bytes; this only guards against huge files.
constexpr size_t kMaxMarkerBytes = 16 * 1024;

// Marker file keys. See low_memory_kill.h for the format.
constexpr std::string_view kForegroundStatusKey = "foreground_status";
constexpr std::string_view kPressureSourceKey = "pressure_source";
constexpr std::string_view kSystemMemoryAvailableKey =
    "system_memory_available";
constexpr std::string_view kSystemMemoryTotalKey = "system_memory_total";
constexpr std::string_view kCobaltMemoryUsageKey = "cobalt_memory_usage";
constexpr std::string_view kCobaltMemoryLimitKey = "cobalt_memory_limit";

// Enum value names, indexed by enum value.
constexpr std::string_view kForegroundStatusNames[] = {
    "unknown",
    "other",
    "foreground",
    "background",
};
static_assert(kSbLowMemoryKillForegroundStatusUnknown == 0 &&
                  kSbLowMemoryKillForegroundStatusOther == 1 &&
                  kSbLowMemoryKillForegroundStatusForeground == 2 &&
                  kSbLowMemoryKillForegroundStatusBackground == 3,
              "kForegroundStatusNames must be indexed by enum value");

constexpr std::string_view kPressureSourceNames[] = {
    "unknown",
    "other",
    "system",
    "cobalt",
};
static_assert(kSbLowMemoryKillPressureSourceUnknown == 0 &&
                  kSbLowMemoryKillPressureSourceOther == 1 &&
                  kSbLowMemoryKillPressureSourceSystem == 2 &&
                  kSbLowMemoryKillPressureSourceCobalt == 3,
              "kPressureSourceNames must be indexed by enum value");

std::string_view Trim(std::string_view s) {
  constexpr char kWhitespace[] = " \t\r";
  size_t begin = s.find_first_not_of(kWhitespace);
  if (begin == std::string_view::npos) {
    return {};
  }
  size_t end = s.find_last_not_of(kWhitespace);
  return s.substr(begin, end - begin + 1);
}

// Parses an unsigned base-10 integer that spans all of |s|. Rejects signs,
// prefixes, trailing characters and values that do not fit in a uint64_t.
bool ParseUint64(std::string_view s, uint64_t* out) {
  if (s.empty()) {
    return false;
  }
  uint64_t value = 0;
  const char* end = s.data() + s.size();
  auto [ptr, ec] = std::from_chars(s.data(), end, value);
  if (ec != std::errc() || ptr != end) {
    return false;
  }
  *out = value;
  return true;
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
  uint64_t value = 0;
  if (ParseUint64(s, &value) && value < N) {
    *out = static_cast<EnumType>(value);
    return true;
  }
  return false;
}

// Returns the name of the enum value stored in |field|, or "unknown" if it is
// out of range. The value is read through its object representation rather
// than loaded as |EnumType|: the struct is a C ABI, C callers may store values
// outside the enum's range, and loading those as a C++ enum is undefined.
template <typename EnumType, size_t N>
std::string_view EnumName(const EnumType& field,
                          const std::string_view (&names)[N]) {
  using Underlying = std::underlying_type_t<EnumType>;
  Underlying raw;
  static_assert(sizeof(raw) == sizeof(field));
  memcpy(&raw, &field, sizeof(raw));
  for (size_t i = 0; i < N; ++i) {
    if (raw == static_cast<Underlying>(i)) {
      return names[i];
    }
  }
  return names[0];  // "unknown"
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

  if (key == kForegroundStatusKey) {
    return ParseEnum(value, kForegroundStatusNames,
                     &details->foreground_status);
  }
  if (key == kPressureSourceKey) {
    return ParseEnum(value, kPressureSourceNames, &details->pressure_source);
  }
  if (key == kSystemMemoryAvailableKey) {
    return ParseUint64(value, &details->system_memory_available);
  }
  if (key == kSystemMemoryTotalKey) {
    return ParseUint64(value, &details->system_memory_total);
  }
  if (key == kCobaltMemoryUsageKey) {
    return ParseUint64(value, &details->cobalt_memory_usage);
  }
  if (key == kCobaltMemoryLimitKey) {
    return ParseUint64(value, &details->cobalt_memory_limit);
  }
  return false;
}

// Reads up to |kMaxMarkerBytes| of the marker at |path| into |contents|.
// Returns false if there is no marker at |path|. |contents| is left empty if
// the marker is not a readable regular file.
// O_NOFOLLOW and O_NONBLOCK keep a symlink or FIFO planted at the marker path
// from redirecting or blocking the read.
bool ReadMarkerContents(const char* path, std::string* contents) {
  contents->clear();
  int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0) {
    // With no marker, the caller must not go on to unlink(): a marker created
    // in between would be consumed without its details having been read.
    // Other errors (e.g. a symlink or an unreadable file) still leave the
    // decision to unlink(), so such a marker counts as a kill without details.
    return errno != ENOENT;
  }
  struct stat info;
  if (fstat(fd, &info) == 0 && S_ISREG(info.st_mode)) {
    char buffer[1024];
    // Read one byte past the limit to detect truncation.
    while (contents->size() <= kMaxMarkerBytes) {
      size_t to_read =
          std::min(sizeof(buffer), kMaxMarkerBytes + 1 - contents->size());
      ssize_t bytes_read = read(fd, buffer, to_read);
      if (bytes_read < 0 && errno == EINTR) {
        continue;
      }
      if (bytes_read <= 0) {
        break;
      }
      contents->append(buffer, static_cast<size_t>(bytes_read));
    }
    if (contents->size() > kMaxMarkerBytes) {
      SB_LOG(WARNING) << "Low memory kill marker exceeds " << kMaxMarkerBytes
                      << " bytes; ignoring the remainder.";
      // Keep only the complete lines within the first |kMaxMarkerBytes|, so a
      // cut-off value is not misread. The extra byte read above shows whether
      // the line ending exactly at the limit is complete.
      size_t last_newline = contents->rfind('\n', kMaxMarkerBytes);
      contents->resize(last_newline == std::string::npos ? 0 : last_newline);
    }
  }
  close(fd);
  return true;
}

// Reads and then removes the marker at |path|. The marker's presence (i.e. a
// successful unlink) is what signals a low memory kill; its contents only
// supply |out_details|.
bool CheckAndConsumeMarker(const char* path,
                           SbLowMemoryKillDetails* out_details) {
  std::string contents;
  if (!ReadMarkerContents(path, &contents)) {
    return false;
  }
  if (unlink(path) != 0) {
    return false;
  }
  *out_details = ParseLowMemoryKillMarker(contents);
  return true;
}

// Implements a two-phase check for possible LMK signals.
//   1. Test Environment Variable Override - allows for easy testing of the LMK
//      API. It carries no details.
//   2. Supervisor/Watchdog Marker File - checks for a breadcrumb file left in
//      /tmp by a memory watchdog process, reads the kill details from it, then
//      deletes the file. See low_memory_kill.h for the file format.
// Note that (1) does not clear any marker files that might exist.
// |out_details| is always reset, and is only populated from a marker.
// The caller must hold a lock on GetMutex() to prevent races for
// CheckAndConsumeMarker().
bool EvaluateWasLowMemoryKilled(SbLowMemoryKillDetails* out_details) {
  *out_details = {};
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

  return CheckAndConsumeMarker(kLowMemoryKillMarkerPath, out_details);
}

// Since an LMK signal may be cleaned up after it is first read, we store the
// results of the first evaluation here and retain them for the life of the
// current process.
// These global variables are protected by the mutex in GetMutex().
static bool g_evaluated = false;
static bool g_was_killed = false;
static SbLowMemoryKillDetails g_details = {};

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

std::string FormatLowMemoryKillMarker(const SbLowMemoryKillDetails& details) {
  std::string out;
  auto append = [&out](std::string_view key, std::string_view value) {
    out.append(key);
    out.push_back('=');
    out.append(value);
    out.push_back('\n');
  };
  append(kForegroundStatusKey,
         EnumName(details.foreground_status, kForegroundStatusNames));
  append(kPressureSourceKey,
         EnumName(details.pressure_source, kPressureSourceNames));
  append(kSystemMemoryAvailableKey,
         std::to_string(details.system_memory_available));
  append(kSystemMemoryTotalKey, std::to_string(details.system_memory_total));
  append(kCobaltMemoryUsageKey, std::to_string(details.cobalt_memory_usage));
  append(kCobaltMemoryLimitKey, std::to_string(details.cobalt_memory_limit));
  return out;
}

bool WriteLowMemoryKillMarker(const char* path,
                              const SbLowMemoryKillDetails& details) {
  if (!path || !*path) {
    return false;
  }
  const std::string contents = FormatLowMemoryKillMarker(details);
  // Write to a uniquely named, exclusively created file in the same directory
  // and rename() it into place, so readers only ever see a complete marker.
  std::string tmp_path = std::string(path) + ".XXXXXX";

  int fd = mkostemp(tmp_path.data(), O_CLOEXEC);
  if (fd < 0) {
    return false;
  }
  bool ok = fchmod(fd, 0644) == 0;
  size_t written = 0;
  while (ok && written < contents.size()) {
    ssize_t result =
        write(fd, contents.data() + written, contents.size() - written);
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result <= 0) {
      ok = false;
      break;
    }
    written += static_cast<size_t>(result);
  }
  if (close(fd) != 0) {
    ok = false;
  }
  if (!ok || rename(tmp_path.c_str(), path) != 0) {
    unlink(tmp_path.c_str());
    return false;
  }
  return true;
}

SbLowMemoryKillDetails ParseLowMemoryKillMarker(std::string_view contents) {
  SbLowMemoryKillDetails details = {};
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

namespace testing {

bool EvaluateLowMemoryKill(SbLowMemoryKillDetails* out_details) {
  auto lock = std::lock_guard(GetMutex());
  SbLowMemoryKillDetails details = {};
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
  g_details = {};
}

}  // namespace testing

}  // namespace starboard
