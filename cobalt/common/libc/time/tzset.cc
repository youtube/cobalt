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

#include "cobalt/common/libc/time/timezone.h"

#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include <array>
#include <charconv>
#include <chrono>
#include <ctime>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "cobalt/common/libc/no_destructor.h"
#include "cobalt/common/libc/tz/parse_posix_tz.h"
#include "starboard/common/log.h"
#include "starboard/log.h"
#include "starboard/time_zone.h"
#include "unicode/basictz.h"
#include "unicode/calendar.h"
#include "unicode/gregocal.h"
#include "unicode/putil.h"
#include "unicode/simpletz.h"
#include "unicode/timezone.h"
#include "unicode/tznames.h"
#include "unicode/tztrans.h"
#include "unicode/ucal.h"
#include "unicode/udata.h"
#include "unicode/unistr.h"
#include "unicode/utypes.h"

namespace cobalt {
namespace common {
namespace libc {
namespace time {

#ifndef TZNAME_MAX
#define TZNAME_MAX 6
#endif

namespace {
// A thread-local flag to detect re-entrant calls to tzset(). This guards
// against infinite recursion or deadlocks that could occur if tzset() is
// called from within itself.
// Our implementation of tzset() calls into ICU. While ICU does not currently
// call back into tzset(), future ICU updates or configuration changes could
// introduce such a dependency. This guard ensures that if a re-entrant call
// ever occurs, it will be caught immediately, preventing a difficult-to-debug
// hang or crash.
thread_local bool g_in_tzset = false;

// A scoped guard to ensure the g_in_tzset flag is reset on exit.
// The tzset() function is not thread-safe by definition, but this guard
// prevents re-entrancy within the same thread, which can happen if a signal
// handler calls a function that directly or indirectly calls tzset().
class TzsetGuard {
 public:
  TzsetGuard() {
    SB_CHECK(!g_in_tzset) << "tzset() re-entrancy detected.";
    g_in_tzset = true;
  }
  ~TzsetGuard() {
    SB_CHECK(g_in_tzset);
    g_in_tzset = false;
  }
};



struct ResolvedTimezone {
  using Name = std::array<char, TZNAME_MAX + 1>;
  Name std_name;
  Name dst_name;
  long timezone_offset = 0;
  int daylight_offset = 0;
  bool is_daylight = false;
};

// A class to encapsulate timezone-related logic and state.
class Timezone {
 public:
  static void Tzset();
  static void SetTmTimezoneFields(struct tm* tm,
                                  const UChar* zone_id,
                                  UDate date);

 private:
  struct Correction {
    const char* std = nullptr;
    const char* dst = nullptr;
    std::optional<int> daylight;
    std::optional<int> timezone;
  };

  using Name = std::array<char, TZNAME_MAX + 1>;

  // Static arrays to hold the strings for tzname[0] and tzname[1].
  static Name std_name_;
  static Name dst_name_;

  // The number of characters to reserve for the cached time zone name.
  static constexpr int kMaxTimezoneNameSize = 128;
  static std::array<char, kMaxTimezoneNameSize> cached_timezone_name_;

  static ResolvedTimezone ResolveTimezone(const char* timezone_name,
                                          std::optional<UDate> date);

  // Copy src to Name without overflowing.
  static void SafeCopyToName(const std::string_view& src,
                             ResolvedTimezone::Name& dest) {
    const size_t chars_to_copy =
        std::min(src.length(), static_cast<size_t>(TZNAME_MAX));
    std::copy_n(src.begin(), chars_to_copy, dest.begin());
    dest[chars_to_copy] = '\0';
  }

  // Helper function to get the current year from ICU's calendar.
  static std::optional<int> GetCurrentYear();

  // Converts a Julian day to a Gregorian month and day for a given year.
  static bool JulianToGregorian(int year,
                                int julian_day,
                                bool is_zero_based,
                                int32_t& out_month,
                                int32_t& out_day);

  // Helper to set a DST transition rule on a SimpleTimeZone object.
  static bool SetTransitionRule(icu::SimpleTimeZone* tz,
                                const tz::TransitionRule& rule,
                                bool is_start_rule);

  // Creates a custom icu::TimeZone from parsed POSIX TZ data.
  static std::unique_ptr<icu::TimeZone> CreateIcuTimezoneFromParsedData(
      const tz::TimezoneData& timezone_data);

  // Applies corrections for specific timezones where the ICU data is incorrect
  // or doesn't match test expectations.
  static void MaybeApplyCorrection(const icu::UnicodeString& time_zone_id,
                                   ResolvedTimezone& resolved);

  // Robustly extracts a timezone name using a layered approach.
  static bool ExtractZoneName(const icu::TimeZone& tz,
                              bool is_daylight,
                              std::optional<UDate> date_for_name,
                              ResolvedTimezone::Name* tz_name);

  // Finds the last date a timezone used DST since 1970 by iterating through
  // all transitions.
  static std::optional<UDate> FindLastDateWithDst(const icu::TimeZone& tz);

  // Return true if the timezone has changed.
  static bool TimezoneIsChanged(const char* timezone_name);

  // Cache the new timezone name.
  static void CacheTimeZoneName(const char* timezone_name);
};

Timezone::Name Timezone::std_name_{};
Timezone::Name Timezone::dst_name_{};
std::array<char, Timezone::kMaxTimezoneNameSize>
    Timezone::cached_timezone_name_{};

// Helper function to get the current year from ICU's calendar.
std::optional<int> Timezone::GetCurrentYear() {
  UErrorCode status = U_ZERO_ERROR;
  icu::GregorianCalendar cal(status);
  if (U_FAILURE(status)) {
    return std::nullopt;
  }
  return cal.get(UCAL_YEAR, status);
}

// Converts a Julian day number to a Gregorian month and day for a given year.
// This is more robust than manual calculations as it leverages ICU's own
// calendar logic.
bool Timezone::JulianToGregorian(int year,
                                 int julian_day,
                                 bool is_zero_based,
                                 int32_t& out_month,
                                 int32_t& out_day) {
  UErrorCode status = U_ZERO_ERROR;
  // Use a Gregorian calendar for the conversion.
  icu::GregorianCalendar cal(status);
  if (U_FAILURE(status)) {
    return false;
  }

  // Set the calendar to the given year.
  cal.set(UCAL_YEAR, year);
  // Set the day of the year. ICU's UCAL_DAY_OF_YEAR is 1-based.
  // The POSIX 'n' format is 0-based, so we add 1.
  cal.set(UCAL_DAY_OF_YEAR, is_zero_based ? julian_day + 1 : julian_day);

  // Get the resulting month and day from the calendar.
  out_month = cal.get(UCAL_MONTH, status);  // UCAL_MONTH is 0-based
  out_day = cal.get(UCAL_DATE, status);     // UCAL_DATE is 1-based

  return U_SUCCESS(status);
}

// Sets a DST transition rule (start or end) on a SimpleTimeZone object based
// on the parsed POSIX rule data. This encapsulates the logic for handling
// Month/Week/Day rules as well as Julian day rules.
bool Timezone::SetTransitionRule(icu::SimpleTimeZone* tz,
                                 const tz::TransitionRule& rule,
                                 bool is_start_rule) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t month;
  int32_t day_of_week = 0;   // Used for MonthWeekDay format
  int32_t day_in_month = 0;  // Used for MonthWeekDay format
  int32_t day_of_month = 0;  // Used for Julian-converted format
  bool use_day_of_month_rule = false;

  switch (rule.date.format) {
    case tz::DateRule::Format::MonthWeekDay: {
      month = rule.date.month - 1;
      day_of_week = rule.date.day + 1;
      day_in_month = (rule.date.week == 5) ? -1 : rule.date.week;
      use_day_of_month_rule = false;
      break;
    }
    case tz::DateRule::Format::Julian:
    case tz::DateRule::Format::ZeroBasedJulian: {
      std::optional<int> current_year_opt = GetCurrentYear();
      if (!current_year_opt) {
        return false;
      }
      const int current_year = *current_year_opt;

      bool is_zero_based =
          (rule.date.format == tz::DateRule::Format::ZeroBasedJulian);
      if (!JulianToGregorian(current_year, rule.date.day, is_zero_based, month,
                             day_of_month)) {
        return false;
      }
      use_day_of_month_rule = true;
      break;
    }
    default:
      return false;  // Unsupported format
  }

  const int32_t time_ms = rule.time * 1000;

  if (is_start_rule) {
    if (use_day_of_month_rule) {
      tz->setStartRule(month, day_of_month, time_ms, status);
    } else {
      tz->setStartRule(month, day_in_month, day_of_week, time_ms, status);
    }
  } else {  // is_end_rule
    if (use_day_of_month_rule) {
      tz->setEndRule(month, day_of_month, time_ms, status);
    } else {
      tz->setEndRule(month, day_in_month, day_of_week, time_ms, status);
    }
  }

  return U_SUCCESS(status);
}

// Creates a custom icu::TimeZone from parsed POSIX TZ data.
// Returns a valid Timezone on success, or nullptr on failure (e.g., if
// the rule format is unsupported).
std::unique_ptr<icu::TimeZone> Timezone::CreateIcuTimezoneFromParsedData(
    const tz::TimezoneData& timezone_data) {
  // POSIX offset is in seconds, positive WEST of GMT.
  // ICU offset is in milliseconds, positive EAST of GMT.
  const int32_t raw_offset_ms = -timezone_data.std_offset * 1000;

  // If there are no DST rules, create a simple, fixed-offset timezone.
  if (!timezone_data.start) {
    return std::make_unique<icu::SimpleTimeZone>(
        raw_offset_ms, icu::UnicodeString::fromUTF8(timezone_data.std));
  }

  // A start rule exists, so an end rule must also exist for valid DST.
  if (!timezone_data.end) {
    return nullptr;
  }

  UErrorCode status = U_ZERO_ERROR;
  auto custom_tz = std::make_unique<icu::SimpleTimeZone>(
      raw_offset_ms, icu::UnicodeString::fromUTF8(timezone_data.std));

  // The DST savings is the difference between the standard and DST offsets.
  // POSIX default is one hour if not explicitly specified.
  const int32_t dst_savings_ms =
      timezone_data.dst_offset
          ? (timezone_data.std_offset - *timezone_data.dst_offset) * 1000
          : 3600 * 1000;  // Default to 1 hour (3600 seconds).
  custom_tz->setDSTSavings(dst_savings_ms, status);
  if (U_FAILURE(status)) {
    return nullptr;
  }

  // Set the start and end rules using the helper function.
  if (!SetTransitionRule(custom_tz.get(), *timezone_data.start,
                         true /* is_start_rule */)) {
    return nullptr;
  }

  if (!SetTransitionRule(custom_tz.get(), *timezone_data.end,
                         false /* is_start_rule */)) {
    return nullptr;
  }

  return custom_tz;
}

// This function is a temporary workaround for the fact that the ICU data
// bundled with Cobalt may be out of date. It applies corrections for timezones
// that have changed since the ICU data was last updated. This function should
// be updated or removed when the ICU data is updated.
void Timezone::MaybeApplyCorrection(const icu::UnicodeString& time_zone_id,
                                    ResolvedTimezone& resolved) {
  // Corrections for certain timezones, necessary since the tzdata in our
  // current ICU is version 2023c.
  static const NoDestructor<std::map<std::string_view, Correction>>
      kCorrections({{
          // "America/Asuncion" was {_}
          {"America/Asuncion", {.timezone = 10800}},
          // "America/Dawson" was {"YST"}
          {"America/Dawson", {.std = "MST"}},
          // "America/Montreal" was {"", ""}
          {"America/Montreal", {.std = "EST", .dst = "EDT"}},
          // "America/Whitehorse" was {"YST"}
          {"America/Whitehorse", {.std = "MST"}},
          // "Asia/Dhaka" was {"BDT"}
          {"Asia/Dhaka", {.std = "BST"}},
          // "Asia/Famagusta" was {"GMT+2", "GMT+3"}
          {"Asia/Famagusta", {.std = "EET", .dst = "EEST"}},
          // "Asia/Manila" was {"PHT"}
          {"Asia/Manila", {.std = "PHST"}},
          // "Europe/Belfast" was {_, "GMT+1"}
          {"Europe/Belfast", {.dst = "BST"}},
          // "Europe/Dublin" was {_, "GMT+1"}
          {"Europe/Dublin", {.dst = "IST"}},  // codespell:ignore ist
          // "Europe/Guernsey" was {_, "GMT+1"}
          {"Europe/Guernsey", {.dst = "BST"}},
          // "Europe/Isle_of_Man" was {_, "GMT+1"}
          {"Europe/Isle_of_Man", {.dst = "BST"}},
          // "Europe/Jersey" was {_, "GMT+1"}
          {"Europe/Jersey", {.dst = "BST"}},
          // "Europe/London" was {_, "GMT+1"}
          {"Europe/London", {.dst = "BST"}},
          // "Canada/Yukon" was {"YST"}
          {"Canada/Yukon", {.std = "MST"}},
          // "CET" was {"GMT", "GMT+1"}
          {"CET", {.std = "CET", .dst = "CEST"}},
          // "EET" was {"GMT+2", "GMT+3"}
          {"EET", {.std = "EET", .dst = "EEST"}},
          // "Eire" was {"GMT+1", ""}
          {"Eire", {.dst = "IST"}},  // codespell:ignore ist
          // "EST" was {"GMT-5"}
          {"EST", {.std = "EST"}},
          // "GB" was {_, "GMT+1"}
          {"GB", {.dst = "BST"}},
          // "GB-Eire" was {_, "GMT+1"}
          {"GB-Eire", {.dst = "BST"}},
          // "WET" was {"GMT", "GMT+1"}
          {"WET", {.std = "WET", .dst = "WEST"}},
      }});

  std::string time_zone_id_str;
  time_zone_id.toUTF8String(time_zone_id_str);

  // Apply corrections from the static table.
  auto it = kCorrections->find(time_zone_id_str);
  if (it != kCorrections->end()) {
    const auto& correction = it->second;
    if (correction.daylight) {
      resolved.daylight_offset = *correction.daylight;
    }
    if (correction.timezone) {
      resolved.timezone_offset = *correction.timezone;
    }
    if (correction.std) {
      std::string_view name(correction.std);
      SafeCopyToName(name, resolved.std_name);
    }
    if (correction.dst) {
      std::string_view name(correction.dst);
      SafeCopyToName(name, resolved.dst_name);
    }
  }
}

bool Timezone::ExtractZoneName(const icu::TimeZone& tz,
                               bool is_daylight,
                               std::optional<UDate> date_for_name,
                               ResolvedTimezone::Name* tz_name) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale locale = icu::Locale::getRoot();
  icu::UnicodeString timezone_id;
  tz.getID(timezone_id);

  std::unique_ptr<icu::TimeZoneNames> tz_db_names(
      icu::TimeZoneNames::createTZDBInstance(locale, status));
  icu::UnicodeString result_name;
  UDate date;

  if (date_for_name) {
    date = *date_for_name;
  } else {
    std::optional<int> current_year_opt = GetCurrentYear();
    if (!current_year_opt) {
      (*tz_name)[0] = '\0';
      return false;
    }
    icu::GregorianCalendar calendar(tz, locale, status);
    calendar.set(*current_year_opt, is_daylight ? UCAL_JULY : UCAL_JANUARY, 1);
    date = calendar.getTime(status);
    if (U_FAILURE(status)) {
      (*tz_name)[0] = '\0';
      return false;
    }
  }

  if (U_SUCCESS(status)) {
    UTimeZoneNameType name_type =
        is_daylight ? UTZNM_SHORT_DAYLIGHT : UTZNM_SHORT_STANDARD;
    tz_db_names->getDisplayName(timezone_id, name_type, date, result_name);
  }

  if (U_FAILURE(status) || result_name.isBogus() || result_name.isEmpty() ||
      result_name.length() > TZNAME_MAX) {
    // The main TZDB lookup failed or returned an empty/invalid name.
    // Try without a date.
    if (date_for_name &&
        ExtractZoneName(tz, is_daylight, std::nullopt, tz_name)) {
      return true;
    }
    // Try the original fallback display name.
    icu::UnicodeString fallback_name;
    tz.getDisplayName(is_daylight, icu::TimeZone::SHORT, locale, fallback_name);
    result_name = fallback_name;
  }

  // Extract the final result.
  if (!result_name.isEmpty() && result_name.length() <= TZNAME_MAX) {
    result_name.extract(0, result_name.length(), tz_name->data(),
                        std::size(*tz_name));
    (*tz_name)[result_name.length()] = '\0';
    return true;
  }

  (*tz_name)[0] = '\0';
  return false;
}

// Finds the last date a timezone used DST since 1970 by manually iterating
// through time. This is necessary because the ICU environment appears to return
// Timezone objects that do not expose their full historical transition data
// via the BasicTimezone interface. This manual iteration is more robust.
std::optional<UDate> Timezone::FindLastDateWithDst(const icu::TimeZone& tz) {
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> cal(
      icu::Calendar::createInstance(tz.clone(), status));
  if (U_FAILURE(status)) {
    return std::nullopt;
  }

  // Set a reasonable end point for the search, e.g., 5 years from now, to
  // account for future DST rule changes.
  icu::GregorianCalendar end_cal(tz, status);
  if (U_FAILURE(status)) {
    return std::nullopt;
  }
  end_cal.setTime(icu::Calendar::getNow(), status);
  if (U_FAILURE(status)) {
    return std::nullopt;
  }
  end_cal.add(UCAL_YEAR, 5, status);
  UDate end_time = end_cal.getTime(status);
  if (U_FAILURE(status)) {
    return std::nullopt;
  }

  // Start search at the beginning of 1970.
  cal->set(1970, UCAL_JANUARY, 1);

  std::optional<UDate> last_dst_date;
  int32_t raw_offset, dst_offset;

  // Iterate week-by-week from 1970 to the end time.
  // This is a robust way to detect if DST was ever active, without
  // relying on the specific Timezone subclass.
  while (cal->getTime(status) < end_time && U_SUCCESS(status)) {
    UDate current_date = cal->getTime(status);
    tz.getOffset(current_date, false, raw_offset, dst_offset, status);
    if (U_FAILURE(status)) {
      break;
    }
    if (dst_offset > 0) {
      // Found a date with DST. Record it and keep searching to find the last
      // one.
      last_dst_date = current_date;
    }
    // Move forward one week. This is granular enough to catch any DST
    // transition without being too slow.
    cal->add(UCAL_WEEK_OF_YEAR, 1, status);
  }

  return last_dst_date;
}

ResolvedTimezone Timezone::ResolveTimezone(const char* timezone_name,
                                           std::optional<UDate> date) {
  ResolvedTimezone resolved;

  // No timezone is specified, default to UTC.
  if (!timezone_name || !timezone_name[0]) {
    constexpr char kUTC[] = "UTC";
    SafeCopyToName(kUTC, resolved.std_name);
    SafeCopyToName(kUTC, resolved.dst_name);
    icu::TimeZone::setDefault(*icu::TimeZone::getGMT());
    return resolved;
  }

  // Attempt to parse the timezone using the POSIX format.
  auto timezone_data = tz::ParsePosixTz(timezone_name);
  if (timezone_data) {
    std::unique_ptr<icu::TimeZone> custom_tz =
        CreateIcuTimezoneFromParsedData(*timezone_data);
    if (custom_tz) {
      icu::TimeZone::setDefault(*custom_tz);
    }

    SafeCopyToName(timezone_data->std, resolved.std_name);
    if (timezone_data->dst) {
      SafeCopyToName(*timezone_data->dst, resolved.dst_name);
      resolved.is_daylight = true;
    } else {
      SafeCopyToName(timezone_data->std, resolved.dst_name);
    }
    resolved.timezone_offset = timezone_data->std_offset;
    resolved.daylight_offset = timezone_data->dst.has_value() ? 1 : 0;
    return resolved;
  }

  // If parsing fails, assume 'timezone_name' is an IANA ID.
  icu::UnicodeString time_zone_id =
      icu::UnicodeString::fromUTF8(timezone_name);
  std::unique_ptr<icu::TimeZone> tz(
      icu::TimeZone::createTimeZone(time_zone_id));

  if (!tz || *tz == icu::TimeZone::getUnknown()) {
    constexpr char kUTC[] = "UTC";
    SafeCopyToName(kUTC, resolved.std_name);
    SafeCopyToName(kUTC, resolved.dst_name);
    icu::TimeZone::setDefault(*icu::TimeZone::getGMT());
    return resolved;
  }

  icu::TimeZone::setDefault(*tz);

  UErrorCode status = U_ZERO_ERROR;
  int32_t raw_offset, dst_offset;
  UDate now = date.value_or(0.0);
  if (date) {
    tz->getOffset(*date, false, raw_offset, dst_offset, status);
  } else {
    now = icu::Calendar::getNow();
    tz->getOffset(now, false, raw_offset, dst_offset, status);
  }

  if (U_FAILURE(status)) {
    constexpr char kUTC[] = "UTC";
    SafeCopyToName(kUTC, resolved.std_name);
    SafeCopyToName(kUTC, resolved.dst_name);
    return resolved;
  }

  resolved.is_daylight = (dst_offset != 0);
  resolved.timezone_offset = -(raw_offset / 1000);
  resolved.daylight_offset = (raw_offset + dst_offset) / 1000;

  std::optional<UDate> last_dst_date = FindLastDateWithDst(*tz);
  ExtractZoneName(*tz, false, std::nullopt, &resolved.std_name);
  if (last_dst_date) {
    resolved.is_daylight = true;
    ExtractZoneName(*tz, true, last_dst_date, &resolved.dst_name);
  } else {
    std::copy(std::begin(resolved.std_name), std::end(resolved.std_name),
              std::begin(resolved.dst_name));
  }

  MaybeApplyCorrection(time_zone_id, resolved);
  return resolved;
}

bool Timezone::TimezoneIsChanged(const char* timezone_name) {
  // If tzname[0] is nullptr, this is the first call. Otherwise,
  // return false if the timezone_name is the same as the cached name.
  if (tzname[0] && timezone_name && timezone_name[0] &&
      strcmp(cached_timezone_name_.data(), timezone_name) == 0) {
    return false;
  }
  return true;
}

void Timezone::CacheTimeZoneName(const char* timezone_name) {
  size_t to_copy = 0;
  if (timezone_name) {
    std::string_view tz_name_view(timezone_name);
    to_copy = std::min(tz_name_view.length(), cached_timezone_name_.size() - 1);
    std::copy_n(tz_name_view.begin(), to_copy, cached_timezone_name_.data());
  }
  cached_timezone_name_[to_copy] = '\0';
}

void Timezone::Tzset() {
  TzsetGuard guard;
  // If the TZ variable is not set, use the system provided timezone.
  const char* timezone_name_str = getenv("TZ");
  if (!timezone_name_str) {
    timezone_name_str = SbTimeZoneGetName();
  }

  if (!TimezoneIsChanged(timezone_name_str)) {
    return;
  }

  CacheTimeZoneName(timezone_name_str);

  ResolvedTimezone resolved = ResolveTimezone(timezone_name_str, std::nullopt);

  timezone = resolved.timezone_offset;
  daylight = resolved.is_daylight;
  SafeCopyToName(std::string_view(resolved.std_name.data()), std_name_);
  SafeCopyToName(std::string_view(resolved.dst_name.data()), dst_name_);
  tzname[0] = std_name_.data();
  tzname[1] = dst_name_.data();
}

void Timezone::SetTmTimezoneFields(struct tm* tm,
                                   const UChar* zone_id,
                                   UDate date) {
  if (!tm) {
    return;
  }

  std::string zone_id_str;
  if (zone_id) {
    icu::UnicodeString(zone_id).toUTF8String(zone_id_str);
  }

  const char* timezone_name_str =
      zone_id ? zone_id_str.c_str() : SbTimeZoneGetName();

  ResolvedTimezone resolved = ResolveTimezone(timezone_name_str, date);

  tm->tm_gmtoff = resolved.daylight_offset;
  static thread_local ResolvedTimezone::Name tz_name_buffer;
  if (resolved.is_daylight) {
    SafeCopyToName(std::string_view(resolved.dst_name.data()), tz_name_buffer);
  } else {
    SafeCopyToName(std::string_view(resolved.std_name.data()), tz_name_buffer);
  }
  tm->tm_zone = tz_name_buffer.data();
}
}  // namespace

void SetTmTimezoneFields(struct tm* tm, const UChar* zone_id, UDate date) {
  Timezone::SetTmTimezoneFields(tm, zone_id, date);
}

}  // namespace time
}  // namespace libc
}  // namespace common
}  // namespace cobalt

// The symbol below must have C linkage.
extern "C" {

long timezone = 0;
int daylight = 0;
char* tzname[2]{nullptr, nullptr};

void tzset(void) {
  cobalt::common::libc::time::Timezone::Tzset();
}

}  // extern "C"
