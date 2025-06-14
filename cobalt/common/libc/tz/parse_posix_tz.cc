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

#include "parse_posix_tz.h"

#include <charconv>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "re2/re2.h"

namespace cobalt {
namespace common {
namespace libc {
namespace tz {

namespace {

std::optional<int> StringViewToInt(std::string_view string_to_convert) {
  int integer_value;
  auto [end_pointer, error_code] = std::from_chars(
      string_to_convert.data(),
      string_to_convert.data() + string_to_convert.size(), integer_value);

  if (error_code == std::errc()) {
    return integer_value;
  }
  return std::nullopt;
}

// Parses a time offset string (e.g., "-8:00:00" or "5") into seconds.
// Returns the offset in seconds west of UTC.
std::optional<TimeOffset> ParseTimeOffset(std::string_view time_offset_string) {
  if (time_offset_string.empty()) {
    return std::nullopt;
  }

  bool is_negative = false;

  // Handle optional leading '+' or '-' sign.
  if (time_offset_string.front() == '+') {
    time_offset_string.remove_prefix(1);
  } else if (time_offset_string.front() == '-') {
    is_negative = true;
    time_offset_string.remove_prefix(1);
  }

  int hours = 0, minutes = 0, seconds = 0;

  // Find the first colon to separate hours.
  size_t first_colon_position = time_offset_string.find(':');
  auto hours_string_view = time_offset_string.substr(0, first_colon_position);
  if (auto parsed_hours = StringViewToInt(hours_string_view); parsed_hours) {
    hours = *parsed_hours;
  } else {
    return std::nullopt;
  }

  // If a colon exists, parse minutes and potentially seconds.
  if (first_colon_position != std::string_view::npos) {
    time_offset_string.remove_prefix(first_colon_position + 1);
    size_t second_colon_position = time_offset_string.find(':');
    auto minutes_string_view =
        time_offset_string.substr(0, second_colon_position);
    if (auto parsed_minutes = StringViewToInt(minutes_string_view);
        parsed_minutes) {
      minutes = *parsed_minutes;
    } else {
      return std::nullopt;
    }

    // If a second colon exists, parse seconds.
    if (second_colon_position != std::string_view::npos) {
      time_offset_string.remove_prefix(second_colon_position + 1);
      if (auto parsed_seconds = StringViewToInt(time_offset_string);
          parsed_seconds) {
        seconds = *parsed_seconds;
      } else {
        return std::nullopt;
      }
    }
  }

  // Calculate the total offset in seconds.
  int total_seconds = hours * 3600 + minutes * 60 + seconds;
  // Invert the sign to get seconds EAST of UTC, a common internal format.
  return is_negative ? -total_seconds : total_seconds;
}

// Parses a date rule string (e.g., "M3.2.0" or "J60") into a DateRule struct.
std::optional<DateRule> ParseDateRule(std::string_view date_rule_string) {
  if (date_rule_string.empty()) {
    return std::nullopt;
  }

  DateRule parsed_date_rule;

  // 'M' format: Month.Week.Day
  if (date_rule_string.front() == 'M') {
    parsed_date_rule.format = DateRule::Format::MonthWeekDay;
    date_rule_string.remove_prefix(1);
    size_t first_dot_position = date_rule_string.find('.');
    size_t second_dot_position =
        date_rule_string.find('.', first_dot_position + 1);

    if (first_dot_position == std::string_view::npos ||
        second_dot_position == std::string_view::npos) {
      return std::nullopt;
    }

    auto month_string_view = date_rule_string.substr(0, first_dot_position);
    auto week_string_view = date_rule_string.substr(
        first_dot_position + 1, second_dot_position - (first_dot_position + 1));
    auto day_string_view = date_rule_string.substr(second_dot_position + 1);

    if (auto parsed_month = StringViewToInt(month_string_view)) {
      parsed_date_rule.month = *parsed_month;
    } else {
      return std::nullopt;
    }
    if (auto parsed_week = StringViewToInt(week_string_view)) {
      parsed_date_rule.week = *parsed_week;
    } else {
      return std::nullopt;
    }
    if (auto parsed_day_of_week = StringViewToInt(day_string_view)) {
      parsed_date_rule.day = *parsed_day_of_week;
    } else {
      return std::nullopt;
    }

    // 'J' format: 1-based Julian day (leap years not counted).
  } else if (date_rule_string.front() == 'J') {
    parsed_date_rule.format = DateRule::Format::Julian;
    date_rule_string.remove_prefix(1);
    if (auto parsed_julian_day = StringViewToInt(date_rule_string)) {
      parsed_date_rule.day = *parsed_julian_day;
    } else {
      return std::nullopt;
    }
    // Default format: 0-based Julian day.
  } else {
    parsed_date_rule.format = DateRule::Format::ZeroBasedJulian;
    if (auto parsed_zero_based_julian_day = StringViewToInt(date_rule_string)) {
      parsed_date_rule.day = *parsed_zero_based_julian_day;
    } else {
      return std::nullopt;
    }
  }
  return parsed_date_rule;
}

// Parses a transition rule string (e.g., "M3.2.0/2:00:00").
std::optional<TransitionRule> ParseTransitionRule(
    std::string_view transition_rule_string) {
  TransitionRule parsed_transition_rule;
  size_t slash_position = transition_rule_string.find('/');

  auto date_rule_string_view = transition_rule_string.substr(0, slash_position);
  if (auto date_rule = ParseDateRule(date_rule_string_view)) {
    parsed_transition_rule.date_rule = *date_rule;
  } else {
    return std::nullopt;
  }

  // Time is optional, defaults to 2:00:00.
  if (slash_position != std::string_view::npos) {
    auto time_offset_string_view =
        transition_rule_string.substr(slash_position + 1);
    if (auto time_offset = ParseTimeOffset(time_offset_string_view)) {
      parsed_transition_rule.time = *time_offset;
    } else {
      return std::nullopt;
    }
  }
  return parsed_transition_rule;
}

}  // namespace

// Parses a POSIX-style timezone string into its component parts.
std::optional<TimezoneData> ParsePosixTz(const std::string& timezone_string) {
  // Regex to capture the components of a POSIX timezone string.
  static const re2::RE2 timezone_posix_regex(
      // Optional : at the start.
      R"(^:?)"
      // G1: Standard time zone name: alphabetic or <quoted>.
      // Quoted names allow alphanumerics, '+', and '-'.
      R"(([A-Za-z]{3,}|<[A-Za-z0-9+-]+>))"
      // G2: Standard time offset.
      R"(([-+]?\d{1,2}(?::\d{2}){0,2}))"
      // Optional DST part: G3 (name) and G4 (offset).
      R"((?:([A-Za-z]{3,}|<[A-Za-z0-9+-]+>)([-+]?\d{1,2}(?::\d{2}){0,2})?)?)"
      // G5: Optional Transition rules.
      R"((?:,(.*))?)"
      R"($)");

  std::vector<re2::StringPiece> regex_captures(
      timezone_posix_regex.NumberOfCapturingGroups() + 1);

  if (!timezone_posix_regex.Match(timezone_string, 0, timezone_string.length(),
                                  re2::RE2::ANCHOR_BOTH, regex_captures.data(),
                                  regex_captures.size())) {
    return std::nullopt;
  }

  TimezoneData parsed_timezone_data;

  // Per POSIX, for a quoted name like "<UTC+3>", the std/dst fields do not
  // include the quoting '<' and '>' characters. The following logic strips
  // them after they are captured by the regex.
  re2::StringPiece std_name_sp = regex_captures[1];
  if (std_name_sp.length() >= 2 && std_name_sp.data()[0] == '<' &&
      std_name_sp.data()[std_name_sp.length() - 1] == '>') {
    parsed_timezone_data.std_name =
        std::string(std_name_sp.data() + 1, std_name_sp.length() - 2);
  } else {
    parsed_timezone_data.std_name = std::string(std_name_sp);
  }

  // The regex ensures that capture group 2 is a valid offset format.
  if (auto offset = ParseTimeOffset(regex_captures[2])) {
    parsed_timezone_data.std_offset = *offset;
  } else {
    return std::nullopt;
  }

  if (regex_captures[3].data() != nullptr) {
    re2::StringPiece dst_name_sp = regex_captures[3];
    if (dst_name_sp.length() >= 2 && dst_name_sp.data()[0] == '<' &&
        dst_name_sp.data()[dst_name_sp.length() - 1] == '>') {
      parsed_timezone_data.dst_name =
          std::string(dst_name_sp.data() + 1, dst_name_sp.length() - 2);
    } else {
      parsed_timezone_data.dst_name = std::string(dst_name_sp);
    }
  }

  // Handle the DST offset, if specified.
  if (regex_captures[4].data() != nullptr && !regex_captures[4].empty()) {
    if (auto offset = ParseTimeOffset(regex_captures[4])) {
      parsed_timezone_data.dst_offset = *offset;
    } else {
      return std::nullopt;
    }
  } else if (parsed_timezone_data.dst_name.has_value()) {
    // If DST name is present but offset isn't, default to one hour
    // ahead of standard time. (e.g., -5hr -> -4hr).
    parsed_timezone_data.dst_offset = parsed_timezone_data.std_offset + 3600;
  }

  // ParsePosixTz transition rules if specified.
  if (regex_captures[5].data() != nullptr) {
    re2::StringPiece rules_string_piece = regex_captures[5];
    // Rules must be a pair of start,end separated by a comma.
    size_t comma_position = rules_string_piece.find(',');
    if (comma_position == re2::StringPiece::npos) {
      return std::nullopt;
    }

    auto start_rule_string_piece = rules_string_piece.substr(0, comma_position);
    std::string_view start_rule_string_view(start_rule_string_piece.data(),
                                            start_rule_string_piece.length());
    if (auto start_rule = ParseTransitionRule(start_rule_string_view)) {
      parsed_timezone_data.start_rule = *start_rule;
    } else {
      return std::nullopt;
    }

    auto end_rule_string_piece = rules_string_piece.substr(comma_position + 1);
    std::string_view end_rule_string_view(end_rule_string_piece.data(),
                                          end_rule_string_piece.length());
    if (auto end_rule = ParseTransitionRule(end_rule_string_view)) {
      parsed_timezone_data.end_rule = *end_rule;
    } else {
      return std::nullopt;
    }
  }

  return parsed_timezone_data;
}

}  // namespace tz
}  // namespace libc
}  // namespace common
}  // namespace cobalt

using cobalt::common::libc::tz::DateRule;
using cobalt::common::libc::tz::TimezoneData;

// Overload for printing DateRule for debugging.
std::ostream& operator<<(std::ostream& out, const DateRule& date_rule) {
  switch (date_rule.format) {
    case DateRule::Format::Julian:
      out << "J" << date_rule.day;
      break;
    case DateRule::Format::ZeroBasedJulian:
      out << date_rule.day;
      break;
    case DateRule::Format::MonthWeekDay:
      out << "M" << date_rule.month << "." << date_rule.week << "."
          << date_rule.day;
      break;
  }
  return out;
}

// Overload for printing the parsed TimezoneData for debugging.
std::ostream& operator<<(
    std::ostream& out,
    const std::optional<TimezoneData>& optional_timezone_data) {
  if (optional_timezone_data) {
    const auto& tz_data = *optional_timezone_data;
    out << "Std Name: " << tz_data.std_name;
    out << " Std Offset: " << tz_data.std_offset;

    if (tz_data.dst_name) {
      out << " Dst Name: " << *tz_data.dst_name;
    }
    if (tz_data.dst_offset) {
      out << " Dst Offset: " << *tz_data.dst_offset;
    }
    if (tz_data.start_rule) {
      out << " Start Rule: " << tz_data.start_rule->date_rule << "/"
          << tz_data.start_rule->time;
    }
    if (tz_data.end_rule) {
      out << " End Rule: " << tz_data.end_rule->date_rule << "/"
          << tz_data.end_rule->time;
    }
  } else {
    out << "Failed to parse timezone string.";
  }
  return out;
}
