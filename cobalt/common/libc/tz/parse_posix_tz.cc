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

#include <algorithm>
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
std::optional<int> ParseInteger(std::string_view& timezone_view) {
  auto it = std::find_if_not(timezone_view.begin(), timezone_view.end(),
                             [](char c) { return std::isdigit(c); });
  size_t digits_end = std::distance(timezone_view.begin(), it);

  if (digits_end == 0) {
    return std::nullopt;
  }

  auto parsed_value = StringViewToInt(timezone_view.substr(0, digits_end));
  if (!parsed_value) {
    return std::nullopt;
  }

  timezone_view.remove_prefix(digits_end);
  return parsed_value;
}

// Parses an optional, colon-prefixed time component (minutes or seconds).
// Returns `true` on success (if the component was parsed, or if it wasn't
// present). Returns `false` on a parse error (e.g., ":" not followed by
// digits). On success, it updates `timezone_view` and `target_component`.
bool ParseOptionalTimeComponent(std::string_view& timezone_view,
                                int& target_component) {
  if (timezone_view.empty() || timezone_view.front() != ':') {
    return true;  // Success: component is optional and not present.
  }

  // We have a ':', so a number must follow.
  timezone_view.remove_prefix(1);
  auto int_res = ParseInteger(timezone_view);
  if (!int_res) {
    // Parse error: ':' was not followed by valid digits.
    return false;
  }
  target_component = *int_res;
  return true;
}

// Parses a time offset string (e.g., "8", "+2", or "-1:30") into seconds.
// On success, returns the offset in seconds and advances the string_view.
// On failure, returns nullopt and the view is not modified.
std::optional<TimeOffset> ParseTimeOffset(std::string_view& timezone_view) {
  if (timezone_view.empty()) {
    return std::nullopt;
  }

  bool is_negative = false;
  if (timezone_view.front() == '+') {
    timezone_view.remove_prefix(1);
  } else if (timezone_view.front() == '-') {
    is_negative = true;
    timezone_view.remove_prefix(1);
  }

  // --- Parse Hours (mandatory) ---
  int total_seconds = 0;
  auto hours_res = ParseInteger(timezone_view);
  if (!hours_res) {
    return std::nullopt;  // Must have hours.
  }
  total_seconds += *hours_res * kSecondsInHour;

  // --- Parse Minutes (optional) ---
  int minutes = 0;
  if (!ParseOptionalTimeComponent(timezone_view, minutes)) {
    return std::nullopt;  // Invalid minutes format.
  }
  total_seconds += minutes * kSecondsInMinute;

  // --- Parse Seconds (optional) ---
  int seconds = 0;
  if (!ParseOptionalTimeComponent(timezone_view, seconds)) {
    return std::nullopt;  // Invalid seconds format.
  }
  total_seconds += seconds;
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
std::optional<std::string> ExtractName(std::string_view& timezone_view) {
  std::string name;

  if (timezone_view.empty()) {
    return std::nullopt;
  }

  if (timezone_view.front() == '<') {
    size_t end_quote = timezone_view.find('>');
    if (end_quote == std::string_view::npos) {
      return std::nullopt;  // Unterminated quoted name.
    }
    name = std::string(timezone_view.substr(1, end_quote - 1));
    timezone_view.remove_prefix(end_quote + 1);
  } else {
    auto it = std::find_if_not(timezone_view.begin(), timezone_view.end(),
                               [](char c) { return std::isalpha(c); });
    size_t name_end = std::distance(timezone_view.begin(), it);

    if (name_end == 0) {
      return std::nullopt;  // No alphabetic name found.
    }
    name = std::string(timezone_view.substr(0, name_end));
    timezone_view.remove_prefix(name_end);
  }
  return name;
}

// Extracts a timezone name and validates it. Returns true if a name was
// extracted. The caller is responsible for checking `is_valid`.
bool ParseZoneName(std::string_view& timezone_view,
                   std::string& name,
                   bool& is_valid) {
  bool is_quoted = (!timezone_view.empty() && timezone_view.front() == '<');
  auto extracted_name = ExtractName(timezone_view);
  if (!extracted_name) {
    return false;
  }
  name = *extracted_name;
  is_valid = !((!is_quoted && name.length() < 3) ||
               name.find('/') != std::string::npos);
  return true;
}

// Parses a time offset. Returns true on success.
bool ParseZoneOffset(std::string_view& timezone_view, TimeOffset& offset) {
  auto parsed_offset = ParseTimeOffset(timezone_view);
  if (!parsed_offset) {
    return false;
  }
  offset = *parsed_offset;
  return true;
}

enum class ZoneParseStatus { kSuccess, kNoName, kInvalidName };

// Attempts to parse a zone name and an optional offset.
// Returns a status indicating the outcome, and the parsed components are
// returned via output parameters.
ZoneParseStatus ParseZone(std::string_view& timezone_view,
                          std::string& out_name,
                          std::optional<TimeOffset>& out_offset) {
  bool is_valid;
  if (!ParseZoneName(timezone_view, out_name, is_valid)) {
    return ZoneParseStatus::kNoName;
  }

  if (!is_valid) {
    return ZoneParseStatus::kInvalidName;
  }

  out_offset.reset();
  TimeOffset offset_val;
  if (ParseZoneOffset(timezone_view, offset_val)) {
    out_offset = offset_val;
  }

  return ZoneParseStatus::kSuccess;
}

// Parses the standard time zone part of the string.
// Returns false if parsing should stop, true otherwise.
bool ParseStd(std::string_view& timezone_view, TimezoneData& result) {
  std::string std_name;
  std::optional<TimeOffset> std_offset;
  ZoneParseStatus std_status = ParseZone(timezone_view, std_name, std_offset);

  if (std_status != ZoneParseStatus::kSuccess || !std_offset.has_value()) {
    // Standard time requires a valid name and an offset.
    return false;
  }
  result.std = std_name;
  result.std_offset = *std_offset;
  return true;
}

// Parses the optional DST part of the time zone string.
// Returns false if parsing should stop, true otherwise.
bool ParseDst(std::string_view& timezone_view, TimezoneData& result) {
  if (timezone_view.empty() || timezone_view.front() == ',') {
    return true;  // No DST part to parse.
  }

  std::string dst_name;
  std::optional<TimeOffset> dst_offset;
  ZoneParseStatus dst_status = ParseZone(timezone_view, dst_name, dst_offset);

  if (dst_status == ZoneParseStatus::kInvalidName) {
    // An invalid DST name is a graceful stop.
    return false;
  }

  if (dst_status == ZoneParseStatus::kSuccess) {
    result.dst = dst_name;
    result.dst_offset = dst_offset.value_or(result.std_offset - kSecondsInHour);
  }

  // After parsing DST, if the remaining string is not empty and doesn't
  // start with a comma, it's a malformed entry.
  if (!timezone_view.empty() && timezone_view.front() != ',') {
    // The DST part was malformed. Discard it and reset the string view.
    result.dst.reset();
    result.dst_offset.reset();
    return false;
  }
  return true;
}

// Section 4: Parse optional transition rules, or apply defaults.
// Returns true to continue parsing, false to stop and return result.
bool ParseTransitionRules(std::string_view& timezone_view,
                          TimezoneData& result) {
  if (!timezone_view.empty() && timezone_view.front() == ',') {
    timezone_view.remove_prefix(1);

    size_t comma_pos = timezone_view.find(',');
    if (comma_pos == std::string_view::npos) {
      // Malformed rule (only one part), so we ignore it and return.
      return false;  // Stop parsing.
    }

    auto start_rule_sv = timezone_view.substr(0, comma_pos);
    auto end_rule_sv = timezone_view.substr(comma_pos + 1);

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
  return true;
}

}  // namespace

// Parses a POSIX-style timezone string into its component parts.
// The format is described as:
// "std[offset[dst[offset]][,start_date[/time],end_date[/time]]]"
std::optional<TimezoneData> ParsePosixTz(const std::string& timezone_string) {
  std::string_view timezone_view(timezone_string);
  TimezoneData result;

  // Skip optional leading ':'
  if (!timezone_view.empty() && timezone_view.front() == ':') {
    timezone_view.remove_prefix(1);
  }

  // 1. Parse Standard Time Name and Offset
  if (!ParseStd(timezone_view, result)) {
    return std::nullopt;
  }

  // 2. Parse optional DST Name and Offset.
  if (!ParseDst(timezone_view, result)) {
    return result;
  }

  // 3. Parse optional transition rules, or apply defaults.
  if (!ParseTransitionRules(timezone_view, result)) {
    return result;
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
