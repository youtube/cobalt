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

#include "cobalt/common/libc/tz/parse_posix_tz.h"

#include <cctype>
#include <charconv>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace cobalt {
namespace common {
namespace libc {
namespace tz {

namespace {

// Number of seconds in an hour.
constexpr int kSecondsInHour = 3600;
// Number of seconds in a minute.
constexpr int kSecondsInMinute = 60;

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

// Parses a sequence of digits from the start of a string_view, advancing the
// view past the digits on success.
// On success, returns the integer value.
// On failure (no digits or invalid number), returns nullopt and the view is
// not modified.
std::optional<int> ParseInteger(std::string_view& sv) {
  size_t digits_end = 0;
  while (digits_end < sv.length() && isdigit(sv[digits_end])) {
    digits_end++;
  }

  if (digits_end == 0) {
    return std::nullopt;
  }

  auto parsed_value = StringViewToInt(sv.substr(0, digits_end));
  if (!parsed_value) {
    return std::nullopt;
  }

  sv.remove_prefix(digits_end);
  return parsed_value;
}

// Parses an optional, colon-prefixed time component (minutes or seconds).
// Returns `true` on success (if the component was parsed, or if it wasn't
// present). Returns `false` on a parse error (e.g., ":" not followed by
// digits). On success, it updates `sv` and `target_component`.
bool ParseOptionalTimeComponent(std::string_view& sv, int& target_component) {
  if (sv.empty() || sv.front() != ':') {
    return true;  // Success: component is optional and not present.
  }

  // We have a ':', so a number must follow.
  auto temp_sv = sv;
  temp_sv.remove_prefix(1);
  auto int_res = ParseInteger(temp_sv);
  if (!int_res) {
    // Parse error: ':' was not followed by valid digits.
    return false;
  }
  target_component = *int_res;
  sv = temp_sv;
  return true;
}

// Parses a time offset string (e.g., "8", "+2", or "-1:30") into seconds.
// On success, returns the offset in seconds and advances the string_view.
// On failure, returns nullopt and the view is not modified.
std::optional<TimeOffset> ParseTimeOffset(std::string_view& sv) {
  auto temp_sv = sv;

  if (temp_sv.empty()) {
    return std::nullopt;
  }

  bool is_negative = false;
  if (temp_sv.front() == '+') {
    temp_sv.remove_prefix(1);
  } else if (temp_sv.front() == '-') {
    is_negative = true;
    temp_sv.remove_prefix(1);
  }

  // --- Parse Hours (mandatory) ---
  int total_seconds = 0;
  auto hours_res = ParseInteger(temp_sv);
  if (!hours_res) {
    return std::nullopt;  // Must have hours.
  }
  total_seconds += *hours_res * kSecondsInHour;

  // --- Parse Minutes (optional) ---
  int minutes = 0;
  if (!ParseOptionalTimeComponent(temp_sv, minutes)) {
    return std::nullopt;  // Invalid minutes format.
  }
  total_seconds += minutes * kSecondsInMinute;

  // --- Parse Seconds (optional) ---
  int seconds = 0;
  if (!ParseOptionalTimeComponent(temp_sv, seconds)) {
    return std::nullopt;  // Invalid seconds format.
  }
  total_seconds += seconds;

  sv = temp_sv;
  return is_negative ? -total_seconds : total_seconds;
}

// Parses a month-week-day format date rule (e.g., "M3.2.0").
std::optional<DateRule> ParseMonthWeekDayRule(std::string_view rule_body) {
  DateRule parsed_date_rule;
  parsed_date_rule.format = DateRule::Format::MonthWeekDay;

  size_t first_dot_position = rule_body.find('.');
  size_t second_dot_position = rule_body.find('.', first_dot_position + 1);

  if (first_dot_position == std::string_view::npos ||
      second_dot_position == std::string_view::npos) {
    return std::nullopt;
  }

  auto month = StringViewToInt(rule_body.substr(0, first_dot_position));
  auto week = StringViewToInt(rule_body.substr(
      first_dot_position + 1, second_dot_position - (first_dot_position + 1)));
  auto day = StringViewToInt(rule_body.substr(second_dot_position + 1));

  if (!month || !week || !day) {
    return std::nullopt;
  }
  parsed_date_rule.month = *month;
  parsed_date_rule.week = *week;
  parsed_date_rule.day = *day;
  return parsed_date_rule;
}

// Parses a Julian day format date rule (e.g., "J60").
std::optional<DateRule> ParseJulianRule(std::string_view rule_body) {
  DateRule parsed_date_rule;
  parsed_date_rule.format = DateRule::Format::Julian;
  auto day = StringViewToInt(rule_body);
  if (!day) {
    return std::nullopt;
  }
  parsed_date_rule.day = *day;
  return parsed_date_rule;
}

// Parses a zero-based Julian day format date rule (e.g., "60").
std::optional<DateRule> ParseZeroBasedJulianRule(std::string_view rule_body) {
  DateRule parsed_date_rule;
  parsed_date_rule.format = DateRule::Format::ZeroBasedJulian;
  auto day = StringViewToInt(rule_body);
  if (!day) {
    return std::nullopt;
  }
  parsed_date_rule.day = *day;
  return parsed_date_rule;
}

// Parses a date rule string (e.g., "M3.2.0" or "J60") into a DateRule struct.
// This function remains strict, as an invalid rule is not recoverable.
std::optional<DateRule> ParseDateRule(std::string_view date_rule_string) {
  if (date_rule_string.empty()) {
    return std::nullopt;
  }

  switch (date_rule_string.front()) {
    case 'M':
      return ParseMonthWeekDayRule(date_rule_string.substr(1));
    case 'J':
      return ParseJulianRule(date_rule_string.substr(1));
    default:
      return ParseZeroBasedJulianRule(date_rule_string);
  }
}

// Parses a transition rule string (e.g., "M3.2.0/2:00:00").
// A transition rule must be fully valid to be accepted.
std::optional<TransitionRule> ParseTransitionRule(
    std::string_view transition_rule_string) {
  TransitionRule parsed_transition_rule;
  size_t slash_position = transition_rule_string.find('/');

  auto date_rule_string_view = transition_rule_string.substr(0, slash_position);
  auto date_rule = ParseDateRule(date_rule_string_view);
  if (!date_rule) {
    return std::nullopt;
  }
  parsed_transition_rule.date = *date_rule;

  // Time is optional, defaults to 2:00:00.
  if (slash_position != std::string_view::npos) {
    auto time_offset_sv = transition_rule_string.substr(slash_position + 1);
    // The time of day for a transition is always positive.
    auto result = ParseTimeOffset(time_offset_sv);
    if (!result || *result < 0 || !time_offset_sv.empty()) {
      return std::nullopt;  // Time part invalid, or junk after time
    }
    parsed_transition_rule.time = *result;
  }
  return parsed_transition_rule;
}

// Helper to extract a timezone name (quoted or unquoted), advancing the view
// past the name on success.
// Returns the name on success, otherwise nullopt. The view is not modified on
// failure.
std::optional<std::string> ExtractName(std::string_view& sv) {
  auto temp_sv = sv;
  std::string name;

  if (temp_sv.empty()) {
    return std::nullopt;
  }

  if (temp_sv.front() == '<') {
    size_t end_quote = temp_sv.find('>');
    if (end_quote == std::string_view::npos) {
      return std::nullopt;  // Unterminated quoted name.
    }
    name = std::string(temp_sv.substr(1, end_quote - 1));
    temp_sv.remove_prefix(end_quote + 1);
  } else {
    size_t name_end = 0;
    // An unquoted name must be alphabetic.
    while (name_end < temp_sv.length() && isalpha(temp_sv[name_end])) {
      name_end++;
    }
    if (name_end == 0) {
      return std::nullopt;  // No alphabetic name found.
    }
    name = std::string(temp_sv.substr(0, name_end));
    temp_sv.remove_prefix(name_end);
  }

  sv = temp_sv;
  return name;
}

}  // namespace

// Parses a POSIX-style timezone string into its component parts.
// The format is described as:
// "std[offset[dst[offset]][,start_date[/time],end_date[/time]]]"
std::optional<TimezoneData> ParsePosixTz(const std::string& timezone_string) {
  std::string_view tz_sv(timezone_string);
  TimezoneData result;

  // Skip optional leading ':'
  if (!tz_sv.empty() && tz_sv.front() == ':') {
    tz_sv.remove_prefix(1);
  }

  // 1. Parse Standard Time Name
  bool is_quoted = (!tz_sv.empty() && tz_sv.front() == '<');
  auto std_name = ExtractName(tz_sv);
  if (!std_name) {
    return std::nullopt;
  }
  if ((!is_quoted && std_name->length() < 3) ||
      std_name->find('/') != std::string::npos) {
    return std::nullopt;
  }
  result.std = *std_name;

  // 2. Parse Standard Time Offset
  auto std_offset = ParseTimeOffset(tz_sv);
  if (!std_offset) {
    return std::nullopt;  // A standard offset is mandatory.
  }
  result.std_offset = *std_offset;

  // 3. Parse optional DST Name and Offset.
  if (!tz_sv.empty() && tz_sv.front() != ',') {
    auto temp_sv = tz_sv;
    auto dst_name = ExtractName(temp_sv);
    if (dst_name) {
      bool is_quoted_dst = (!tz_sv.empty() && tz_sv.front() == '<');
      if ((!is_quoted_dst && dst_name->length() < 3) ||
          dst_name->find('/') != std::string::npos) {
        return result;
      }

      result.dst = *dst_name;

      if (!temp_sv.empty() && temp_sv.front() != ',') {
        auto dst_offset = ParseTimeOffset(temp_sv);
        if (!dst_offset) {
          return result;
        }
        result.dst_offset = dst_offset;
      } else {
        result.dst_offset = result.std_offset - kSecondsInHour;
      }
      tz_sv = temp_sv;
    }
  }

  // 4. Parse optional transition rules, or apply defaults.
  if (!tz_sv.empty() && tz_sv.front() == ',') {
    tz_sv.remove_prefix(1);

    size_t comma_pos = tz_sv.find(',');
    if (comma_pos == std::string_view::npos) {
      // Malformed rule (only one part), so we ignore it and return what we
      // have.
      return result;
    }

    auto start_rule_sv = tz_sv.substr(0, comma_pos);
    auto end_rule_sv = tz_sv.substr(comma_pos + 1);

    auto start_rule = ParseTransitionRule(start_rule_sv);
    auto end_rule = ParseTransitionRule(end_rule_sv);

    if (start_rule && end_rule) {
      result.start = start_rule;
      result.end = end_rule;
    }
  } else if (result.dst.has_value()) {
    // If a DST name exists but no rules were specified, apply US default rules.
    // This is a common implementation choice for POSIX compliance.
    // Starts second Sunday in March at 2am.
    result.start = TransitionRule{{DateRule::Format::MonthWeekDay, 3, 2, 0},
                                  2 * kSecondsInHour};
    // Ends first Sunday in November at 2am.
    result.end = TransitionRule{{DateRule::Format::MonthWeekDay, 11, 1, 0},
                                2 * kSecondsInHour};
  }

  return result;
}

std::ostream& operator<<(std::ostream& out, const DateRule& rule) {
  out << "{ format: ";
  switch (rule.format) {
    case DateRule::Format::MonthWeekDay:
      out << "MonthWeekDay, m: " << rule.month << ", w: " << rule.week
          << ", d: " << rule.day;
      break;
    case DateRule::Format::Julian:
      out << "Julian, day: " << rule.day;
      break;
    case DateRule::Format::ZeroBasedJulian:
      out << "ZeroBasedJulian, day: " << rule.day;
      break;
  }
  out << " }";
  return out;
}

std::ostream& operator<<(std::ostream& out, const TimezoneData& data) {
  out << "{ std: \"" << data.std << "\", std_offset: " << data.std_offset;

  out << ", dst: ";
  if (data.dst) {
    out << "\"" << *data.dst << "\"";
  } else {
    out << "null";
  }

  out << ", dst_offset: ";
  if (data.dst_offset) {
    out << *data.dst_offset;
  } else {
    out << "null";
  }

  out << ", start: ";
  if (data.start) {
    out << *data.start;
  } else {
    out << "null";
  }

  out << ", end: ";
  if (data.end) {
    out << *data.end;
  } else {
    out << "null";
  }

  out << " }";
  return out;
}

std::ostream& operator<<(
    std::ostream& out,
    const std::optional<TimezoneData>& optional_timezone_data) {
  if (optional_timezone_data) {
    out << *optional_timezone_data;
  } else {
    out << "Failed to parse timezone string.";
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const TransitionRule& rule) {
  out << "{ date: " << rule.date << ", time: " << rule.time << "s }";
  return out;
}

}  // namespace tz
}  // namespace libc
}  // namespace common
}  // namespace cobalt