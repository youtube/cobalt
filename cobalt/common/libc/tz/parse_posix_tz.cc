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

// A helper struct to return both a parsed value and the number of characters
// consumed from the input string. This is crucial for the iterative parser
// to know where to resume parsing.
template <typename T>
struct ParseResult {
  T value;
  size_t consumed;
};

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

// Parses a sequence of digits from the start of a string_view.
// On success, returns a ParseResult with the integer value and number of
// characters consumed. On failure (no digits or invalid number), returns
// nullopt. This function does not modify the input string_view.
std::optional<ParseResult<int>> ParseInteger(std::string_view sv) {
  size_t digits_end = 0;
  while (digits_end < sv.length() && isdigit(sv[digits_end])) {
    digits_end++;
  }

  if (digits_end == 0) {
    return std::nullopt;
  }

  if (auto parsed_value = StringViewToInt(sv.substr(0, digits_end))) {
    return {{*parsed_value, digits_end}};
  }

  return std::nullopt;
}

// Parses an optional, colon-prefixed time component (minutes or seconds).
// Returns `true` on success (if the component was parsed, or if it wasn't
// present). Returns `false` on a parse error (e.g., ":" not followed by
// digits). On success, it updates `sv`, `consumed`, and `target_component`.
bool ParseOptionalTimeComponent(std::string_view& sv,
                                size_t& consumed,
                                int& target_component) {
  if (sv.empty() || sv.front() != ':') {
    return true;  // Success: component is optional and not present.
  }

  // We have a ':', so a number must follow.
  if (auto int_res = ParseInteger(sv.substr(1))) {
    target_component = int_res->value;
    // Consume the ':' plus the digits.
    size_t component_consumed = 1 + int_res->consumed;
    sv.remove_prefix(component_consumed);
    consumed += component_consumed;
    return true;
  }

  // Parse error: ':' was not followed by valid digits.
  return false;
}

// Parses a time offset string (e.g., "8", "+2", or "-1:30") into seconds.
// Returns the offset in seconds and the number of characters consumed.
std::optional<ParseResult<TimeOffset>> ParseTimeOffset(
    std::string_view time_offset_string) {
  if (time_offset_string.empty()) {
    return std::nullopt;
  }

  bool is_negative = false;
  size_t consumed = 0;

  // Handle optional leading '+' or '-' sign.
  if (time_offset_string.front() == '+') {
    time_offset_string.remove_prefix(1);
    consumed++;
  } else if (time_offset_string.front() == '-') {
    is_negative = true;
    time_offset_string.remove_prefix(1);
    consumed++;
  }

  // --- Parse Hours (mandatory) ---
  int total_seconds = 0;
  if (auto hours_res = ParseInteger(time_offset_string)) {
    total_seconds += hours_res->value * kSecondsInHour;
    consumed += hours_res->consumed;
    time_offset_string.remove_prefix(hours_res->consumed);
  } else {
    return std::nullopt;  // Must have hours
  }

  // --- Parse Minutes (optional) ---
  int minutes = 0;
  if (ParseOptionalTimeComponent(time_offset_string, consumed, minutes)) {
    total_seconds += minutes * kSecondsInMinute;
    // --- Parse Seconds (optional, only if minutes were parsable) ---
    int seconds = 0;
    if (ParseOptionalTimeComponent(time_offset_string, consumed, seconds)) {
      total_seconds += seconds;
    }
  }

  return {{is_negative ? -total_seconds : total_seconds, consumed}};
}

// Parses a date rule string (e.g., "M3.2.0" or "J60") into a DateRule struct.
// This function remains strict, as an invalid rule is not recoverable.
std::optional<DateRule> ParseDateRule(std::string_view date_rule_string) {
  if (date_rule_string.empty()) {
    return std::nullopt;
  }

  DateRule parsed_date_rule;
  switch (date_rule_string.front()) {
    case 'M': {
      parsed_date_rule.format = DateRule::Format::MonthWeekDay;
      date_rule_string.remove_prefix(1);
      size_t first_dot_position = date_rule_string.find('.');
      size_t second_dot_position =
          date_rule_string.find('.', first_dot_position + 1);

      if (first_dot_position == std::string_view::npos ||
          second_dot_position == std::string_view::npos) {
        return std::nullopt;
      }

      auto month =
          StringViewToInt(date_rule_string.substr(0, first_dot_position));
      auto week = StringViewToInt(date_rule_string.substr(
          first_dot_position + 1,
          second_dot_position - (first_dot_position + 1)));
      auto day =
          StringViewToInt(date_rule_string.substr(second_dot_position + 1));

      if (month && week && day) {
        parsed_date_rule.month = *month;
        parsed_date_rule.week = *week;
        parsed_date_rule.day = *day;
      } else {
        return std::nullopt;
      }
      break;
    }
    case 'J': {
      parsed_date_rule.format = DateRule::Format::Julian;
      date_rule_string.remove_prefix(1);
      if (auto day = StringViewToInt(date_rule_string)) {
        parsed_date_rule.day = *day;
      } else {
        return std::nullopt;
      }
      break;
    }
    default: {
      parsed_date_rule.format = DateRule::Format::ZeroBasedJulian;
      if (auto day = StringViewToInt(date_rule_string)) {
        parsed_date_rule.day = *day;
      } else {
        return std::nullopt;
      }
      break;
    }
  }
  return parsed_date_rule;
}

// Parses a transition rule string (e.g., "M3.2.0/2:00:00").
// A transition rule must be fully valid to be accepted.
std::optional<TransitionRule> ParseTransitionRule(
    std::string_view transition_rule_string) {
  TransitionRule parsed_transition_rule;
  size_t slash_position = transition_rule_string.find('/');

  auto date_rule_string_view = transition_rule_string.substr(0, slash_position);
  if (auto date_rule = ParseDateRule(date_rule_string_view)) {
    parsed_transition_rule.date = *date_rule;
  } else {
    return std::nullopt;
  }

  // Time is optional, defaults to 2:00:00.
  if (slash_position != std::string_view::npos) {
    auto time_offset_string_view =
        transition_rule_string.substr(slash_position + 1);
    // The time of day for a transition is always positive.
    if (auto result = ParseTimeOffset(time_offset_string_view);
        result && result->value >= 0) {
      parsed_transition_rule.time = result->value;
    } else {
      return std::nullopt;  // Time part is invalid or negative.
    }
  }
  return parsed_transition_rule;
}

// Helper to extract a timezone name (quoted or unquoted).
// Returns the name and the number of characters consumed.
ParseResult<std::string> ExtractName(std::string_view sv) {
  std::string name;
  size_t consumed = 0;

  if (sv.empty()) {
    return {name, consumed};
  }

  if (sv.front() == '<') {
    size_t end_quote = sv.find('>');
    if (end_quote != std::string_view::npos) {
      name = std::string(sv.substr(1, end_quote - 1));
      consumed = end_quote + 1;
    }
  } else {
    size_t name_end = 0;
    // An unquoted name must be alphabetic.
    while (name_end < sv.length() && isalpha(sv[name_end])) {
      name_end++;
    }
    if (name_end > 0) {
      name = std::string(sv.substr(0, name_end));
      consumed = name_end;
    }
  }
  return {name, consumed};
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
  auto std_name_res = ExtractName(tz_sv);
  // Unquoted POSIX names must be at least 3 characters.
  bool is_quoted = (!tz_sv.empty() && tz_sv.front() == '<');
  if (std_name_res.consumed == 0 ||
      (!is_quoted && std_name_res.value.length() < 3)) {
    return std::nullopt;
  }
  // IANA names contain '/', which is not allowed in POSIX names.
  if (std_name_res.value.find('/') != std::string::npos) {
    return std::nullopt;
  }
  result.std = std_name_res.value;
  tz_sv.remove_prefix(std_name_res.consumed);

  // 2. Parse Standard Time Offset
  auto std_offset_res = ParseTimeOffset(tz_sv);
  if (!std_offset_res) {
    return std::nullopt;  // A standard offset is mandatory.
  }
  result.std_offset = std_offset_res->value;
  tz_sv.remove_prefix(std_offset_res->consumed);

  // 3. Parse optional DST Name and Offset.
  // This block parses the `dst[offset]` part atomically. It will only
  // commit the DST information to the result if the name and its optional
  // offset are both valid.
  auto dst_name_res = ExtractName(tz_sv);
  if (dst_name_res.consumed > 0) {
    // Re-evaluate is_quoted for the DST name.
    is_quoted = (!tz_sv.empty() && tz_sv.front() == '<');

    // Per POSIX, unquoted names must be >= 3 chars.
    // This implementation also disallows '/' in names to avoid IANA-style
    // names.
    if ((!is_quoted && dst_name_res.value.length() < 3) ||
        dst_name_res.value.find('/') != std::string::npos) {
      return result;  // Invalid name format, stop parsing.
    }

    auto tz_sv_after_name = tz_sv;
    tz_sv_after_name.remove_prefix(dst_name_res.consumed);

    std::optional<TimeOffset> dst_offset;
    size_t offset_consumed = 0;

    // A DST name can be followed by an offset or directly by rules (a ',').
    if (!tz_sv_after_name.empty() && tz_sv_after_name.front() != ',') {
      auto dst_offset_res = ParseTimeOffset(tz_sv_after_name);
      if (dst_offset_res) {
        dst_offset = dst_offset_res->value;
        offset_consumed = dst_offset_res->consumed;
      } else {
        // Found a name-like string, but it's followed by something that is
        // not a valid offset nor a rule delimiter. Stop parsing here.
        return result;
      }
    } else {
      // If DST name is present but offset isn't, default to one hour
      // less than standard time offset. (e.g. EST5EDT, EST is +5h, EDT is +4h)
      dst_offset = result.std_offset - kSecondsInHour;
    }

    // If we've successfully parsed a DST name and its offset (explicit or
    // default), commit it to the results and advance the parser.
    result.dst = dst_name_res.value;
    result.dst_offset = dst_offset;
    tz_sv.remove_prefix(dst_name_res.consumed + offset_consumed);
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

    // Both rules must be fully valid to be included.
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
