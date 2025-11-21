// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_COMMON_LIBC_TZ_PARSE_POSIX_TZ_H_
#define COBALT_COMMON_LIBC_TZ_PARSE_POSIX_TZ_H_

#include <optional>
#include <string>

namespace cobalt {
namespace common {
namespace libc {
namespace tz {

// TimeOffset is an alias for an integer holding the total offset from UTC in
// seconds. Positive for timezones west of GMT (e.g., America/Los_Angeles, PST
// is +8h = 28800). Negative for timezones EAST of GMT (e.g., Europe/Paris, CET
// is -1h = -3600).
using TimeOffset = int;

// Represents a date rule for DST transitions
struct DateRule {
  // The format is nested inside the struct that uses it
  enum class Format {
    Julian,           // Jn
    ZeroBasedJulian,  // n
    MonthWeekDay      // Mm.w.d
  };

  Format format;
  int month = 0;
  int week = 0;
  int day = 0;
};

// Represents a complete transition rule (date and time)
struct TransitionRule {
  DateRule date;
  TimeOffset time = 2 * 3600;  // Default time is 02:00:00 (per POSIX).
};

// Top-level structure for the parsed TZ data
struct TimezoneData {
  std::string std;
  TimeOffset std_offset;
  std::optional<std::string> dst;
  std::optional<TimeOffset> dst_offset;
  std::optional<TransitionRule> start;
  std::optional<TransitionRule> end;
};

// Parses the given string in POSIX TZ format.
std::optional<TimezoneData> ParsePosixTz(const std::string& tz_string);

// Stream operator overloads to allow printing of the structs.
std::ostream& operator<<(std::ostream& out, const DateRule& rule);

std::ostream& operator<<(std::ostream& out, const TimezoneData& data);
std::ostream& operator<<(std::ostream& out,
                         const std::optional<TimezoneData>& tz_data);

std::ostream& operator<<(std::ostream& out, const TransitionRule& rule);

}  // namespace tz
}  // namespace libc
}  // namespace common
}  // namespace cobalt

#endif  // COBALT_COMMON_LIBC_TZ_PARSE_POSIX_TZ_H_
