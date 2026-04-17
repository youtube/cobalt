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

#include "cobalt/common/libc/time/time_zone_state.h"

#include <limits.h>
#include <string.h>
#include <time.h>

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <ctime>
#include <map>
#include <memory>
#include <optional>
#include <string>
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

namespace {

// Number of seconds in an hour.
constexpr int kSecondsInHour = 3600;

// Safely copies a string_view to a character buffer, ensuring null
// termination and preventing buffer overflows.
void SafeCopyToName(const std::string_view& src, char* dest, size_t dest_size) {
  const size_t chars_to_copy = std::min(src.length(), dest_size - 1);
  std::copy_n(src.begin(), chars_to_copy, dest);
  dest[chars_to_copy] = '\0';
}

// Retrieves the current year using ICU's Gregorian calendar based on the
// system's clock. Returns nullopt on failure.
std::optional<int> GetCurrentYear() {
  UErrorCode status = U_ZERO_ERROR;
  icu::GregorianCalendar cal(status);
  if (U_FAILURE(status)) {
    return std::nullopt;
  }
  return cal.get(UCAL_YEAR, status);
}

// Converts a Julian day of the year to a Gregorian month and day.
// The Julian day can be 1-based (day 1-366) or 0-based (day 0-365).
bool JulianToGregorian(int year,
                       int julian_day,
                       bool is_zero_based,
                       int32_t& out_month,
                       int32_t& out_day) {
  UErrorCode status = U_ZERO_ERROR;
  icu::GregorianCalendar cal(status);
  if (U_FAILURE(status)) {
    return false;
  }

  cal.set(UCAL_YEAR, year);
  cal.set(UCAL_DAY_OF_YEAR, is_zero_based ? julian_day + 1 : julian_day);

  out_month = cal.get(UCAL_MONTH, status);
  out_day = cal.get(UCAL_DATE, status);

  return U_SUCCESS(status);
}

// Configures a daylight saving time (DST) transition rule on an
// icu::SimpleTimeZone object. This function handles different date rule formats
// (Month-Week-Day or Julian day) and sets either the DST start or end time.
// The rule's date format determines how the transition is specified:
// - MonthWeekDay: e.g., the second Sunday in March.
// - Julian: The Nth day of the year.
// The function translates these formats into parameters compatible with ICU's
// setStartRule/setEndRule methods.
bool SetTransitionRule(icu::SimpleTimeZone* tz,
                       const tz::TransitionRule& rule,
                       bool is_start_rule) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t month;
  int32_t day_of_week = 0;
  int32_t day_in_month = 0;
  int32_t day_of_month = 0;
  bool use_day_of_month_rule = false;

  switch (rule.date.format) {
    case tz::DateRule::Format::MonthWeekDay: {
      // ICU months are 0-based, and days of the week are 1-based (Sunday=1).
      month = rule.date.month - 1;
      day_of_week = rule.date.day + 1;
      // A week value of 5 means the last week of the month, which ICU
      // represents as -1.
      day_in_month = (rule.date.week == 5) ? -1 : rule.date.week;
      use_day_of_month_rule = false;
      break;
    }
    case tz::DateRule::Format::Julian:
    case tz::DateRule::Format::ZeroBasedJulian: {
      // Julian dates must be converted to a month and day for the current year
      // to create a rule that ICU can apply annually.
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
      return false;
  }

  // The transition time is specified in seconds, so convert to milliseconds.
  const int32_t time_ms = rule.time * 1000;

  if (is_start_rule) {
    if (use_day_of_month_rule) {
      tz->setStartRule(month, day_of_month, time_ms, status);
    } else {
      tz->setStartRule(month, day_in_month, day_of_week, time_ms, status);
    }
  } else {
    if (use_day_of_month_rule) {
      tz->setEndRule(month, day_of_month, time_ms, status);
    } else {
      tz->setEndRule(month, day_in_month, day_of_week, time_ms, status);
    }
  }

  return U_SUCCESS(status);
}

// Constructs a custom icu::TimeZone object from parsed POSIX TZ string data.
// This function sets the standard time offset and configures DST rules if they
// are specified. If a DST zone is named but no transition rules are given, it
// defaults to the standard US DST rules.
std::unique_ptr<icu::TimeZone> CreateIcuTimezoneFromParsedData(
    const tz::TimezoneData& timezone_data) {
  // The raw offset is the difference from UTC in milliseconds.
  const int32_t raw_offset_ms = -timezone_data.std_offset * 1000;

  auto custom_tz = std::make_unique<icu::SimpleTimeZone>(
      raw_offset_ms, icu::UnicodeString::fromUTF8(timezone_data.std));

  // Handle cases like "EST5EDT" where DST is implied but no rules are given.
  if (!timezone_data.start && timezone_data.dst) {
    // If a DST zone is specified without rules, apply default US rules:
    // Start: Second Sunday in March at 2 AM.
    // End: First Sunday in November at 2 AM.
    UErrorCode status = U_ZERO_ERROR;
    custom_tz->setStartRule(UCAL_MARCH, 2, UCAL_SUNDAY,
                            2 * kSecondsInHour * 1000, status);
    custom_tz->setEndRule(UCAL_NOVEMBER, 1, UCAL_SUNDAY,
                          2 * kSecondsInHour * 1000, status);
    if (U_FAILURE(status)) {
      return nullptr;
    }
  }

  // If there's no end rule, there's no DST to configure.
  if (!timezone_data.end) {
    return custom_tz;
  }

  UErrorCode status = U_ZERO_ERROR;
  // Calculate the DST savings. If a dst_offset is provided, use it.
  // Otherwise, default to a one-hour saving.
  const int32_t dst_savings_ms =
      timezone_data.dst_offset
          ? (timezone_data.std_offset - *timezone_data.dst_offset) * 1000
          : kSecondsInHour * 1000;
  custom_tz->setDSTSavings(dst_savings_ms, status);
  if (U_FAILURE(status)) {
    return nullptr;
  }

  // Set the start and end transition rules for DST.
  if (!SetTransitionRule(custom_tz.get(), *timezone_data.start, true)) {
    return nullptr;
  }

  if (!SetTransitionRule(custom_tz.get(), *timezone_data.end, false)) {
    return nullptr;
  }

  return custom_tz;
}

// Extracts the timezone abbreviation (e.g., "PST", "PDT") for a given
// timezone. It uses a layered approach, first querying the TZDB for a name and
// falling back to a generic name if the primary lookup fails. The optional
// `date_for_name` parameter allows specifying a historical date to get the
// correct abbreviation for that time (e.g., for zones that no longer use DST).
std::string ExtractZoneName(const icu::TimeZone& tz,
                            bool is_daylight,
                            std::optional<UDate> date_for_name) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale locale = icu::Locale::getRoot();
  icu::UnicodeString timezone_id;
  tz.getID(timezone_id);

  // First, try to get the name from the IANA Time Zone Database (TZDB).
  // This provides the most accurate and context-specific names.
  std::unique_ptr<icu::TimeZoneNames> tz_db_names(
      icu::TimeZoneNames::createTZDBInstance(locale, status));
  icu::UnicodeString result_name;
  UDate date;

  if (date_for_name) {
    date = *date_for_name;
  } else {
    // If no specific date is provided, use a representative date for the
    // current year (January for standard time, July for daylight time).
    std::optional<int> current_year_opt = GetCurrentYear();
    if (!current_year_opt) {
      return "";
    }
    icu::GregorianCalendar calendar(tz, locale, status);
    calendar.set(*current_year_opt, is_daylight ? UCAL_JULY : UCAL_JANUARY, 1);
    date = calendar.getTime(status);
    if (U_FAILURE(status)) {
      return "";
    }
  }

  if (U_SUCCESS(status)) {
    UTimeZoneNameType name_type =
        is_daylight ? UTZNM_SHORT_DAYLIGHT : UTZNM_SHORT_STANDARD;
    tz_db_names->getDisplayName(timezone_id, name_type, date, result_name);
  }

  // If the TZDB lookup fails or returns an invalid name, fall back.
  if (U_FAILURE(status) || result_name.isBogus() || result_name.isEmpty() ||
      result_name.length() > TZNAME_MAX) {
    // If we already tried a specific date, retry with a generic one.
    if (date_for_name) {
      return ExtractZoneName(tz, is_daylight, std::nullopt);
    }
    // Use ICU's generic display name as a final fallback.
    icu::UnicodeString fallback_name;
    tz.getDisplayName(is_daylight, icu::TimeZone::SHORT, locale, fallback_name);
    result_name = fallback_name;
  }

  if (!result_name.isEmpty() && result_name.length() <= TZNAME_MAX) {
    std::string name_str;
    result_name.toUTF8String(name_str);
    return name_str;
  }

  return "";
}

// Finds the last date a timezone observed Daylight Saving Time since 1970.
// This is useful for determining the correct DST abbreviation for timezones
// that have since abandoned DST. For example, if a zone stopped using DST in
// 2005, this function helps retrieve its historical DST name (e.g., "PDT")
// instead of its current standard name (e.g., "PST").
std::optional<UDate> FindLastDateWithDst(const icu::TimeZone& tz) {
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> cal(
      icu::Calendar::createInstance(tz.clone(), status));
  if (U_FAILURE(status)) {
    return std::nullopt;
  }

  // Set a search end time a few years in the future to catch recent changes.
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

  // Start searching from the Unix epoch.
  cal->set(1970, UCAL_JANUARY, 1);

  std::optional<UDate> last_dst_date;
  int32_t raw_offset, dst_offset;

  // Iterate week by week to find periods with a non-zero DST offset.
  while (cal->getTime(status) < end_time && U_SUCCESS(status)) {
    UDate current_date = cal->getTime(status);
    tz.getOffset(current_date, false, raw_offset, dst_offset, status);
    if (U_FAILURE(status)) {
      break;
    }
    if (dst_offset > 0) {
      last_dst_date = current_date;
    }
    cal->add(UCAL_WEEK_OF_YEAR, 1, status);
  }

  return last_dst_date;
}

}  // namespace

char TimeZoneState::std_name_buffer_[TZNAME_MAX + 1];
char TimeZoneState::dst_name_buffer_[TZNAME_MAX + 1];

TimeZoneState::TimeZoneState() {
  EnsureTimeZoneIsCreated();
}

TimeZoneState::~TimeZoneState() = default;

std::shared_ptr<const icu::TimeZone> TimeZoneState::GetTimeZone() {
  EnsureTimeZoneIsCreated();
  return std::atomic_load(&current_zone_);
}

void TimeZoneState::GetPosixTimezoneGlobals(long& out_timezone,
                                            int& out_daylight,
                                            char** out_tzname) {
  EnsureTimeZoneIsCreated();
  auto tz = std::atomic_load(&current_zone_);
  out_timezone = -(tz->getRawOffset() / 1000);
  out_daylight = tz->useDaylightTime();
  SafeCopyToName(std_name_, std_name_buffer_, sizeof(std_name_buffer_));
  SafeCopyToName(dst_name_, dst_name_buffer_, sizeof(dst_name_buffer_));
  out_tzname[0] = std_name_buffer_;
  out_tzname[1] = dst_name_buffer_;
}

std::unique_ptr<icu::TimeZone> TimeZoneState::CreateTimeZoneFromIanaId(
    const std::string& tz_id) {
  // The Correction struct and kCorrections map are used to handle
  // inconsistencies or missing data in the underlying timezone database for
  // specific zones. This provides a manual override mechanism.
  struct Correction {
    const char* std;
    const char* dst;
    bool dst_active;
    std::optional<int> offset;
  };
  static const NoDestructor<std::map<std::string, Correction>> kCorrections({
      {"America/Asuncion", {"PYT", "PYST", false, 3 * kSecondsInHour}},
      {"America/Dawson", {"MST", "MST", false}},
      {"America/Montreal", {"EST", "EDT", true}},
      {"America/Whitehorse", {"MST", "MST", false}},
      {"Canada/Yukon", {"MST", "MST", false}},
      {"Asia/Dhaka", {"BST", "BST", false}},
      {"Asia/Famagusta", {"EET", "EEST", true}},
      {"Asia/Manila", {"PHST", "PHST", false}},
      {"Europe/Belfast", {"GMT", "BST", true}},
      {"Europe/Dublin", {"GMT", "IST", true}},
      {"Europe/Guernsey", {"GMT", "BST", true}},
      {"Europe/Isle_of_Man", {"GMT", "BST", true}},
      {"Europe/Jersey", {"GMT", "BST", true}},
      {"Europe/London", {"GMT", "BST", true}},
      {"CET", {"CET", "CEST", true}},
      {"EET", {"EET", "EEST", true}},
      {"Eire", {"GMT", "IST", true}},
      {"EST", {"EST", "EST", false}},
      {"EST5EDT", {"EST", "EDT", true, 5 * kSecondsInHour}},
      {"GB-Eire", {"GMT", "BST", true}},
      {"GB", {"GMT", "BST", true}},
      {"MST7MDT", {"MST", "MDT", true, 7 * kSecondsInHour}},
      {"WET", {"WET", "WEST", true}},
  });

  icu::UnicodeString time_zone_id = icu::UnicodeString::fromUTF8(tz_id);
  auto* tz = icu::TimeZone::createTimeZone(time_zone_id);
  std::unique_ptr<icu::TimeZone> new_zone;

  // If the provided ID is not recognized, fall back to UTC.
  if (!tz || *tz == icu::TimeZone::getUnknown()) {
    delete tz;
    new_zone.reset(icu::TimeZone::createTimeZone("UTC"));
  } else {
    new_zone.reset(tz);
  }

  // Check if there's a manual correction for this timezone ID.
  auto it = kCorrections->find(tz_id);
  if (it != kCorrections->end()) {
    const auto& correction = it->second;
    std_name_ = correction.std;
    dst_name_ = correction.dst;

    int32_t corrected_raw_offset_ms = new_zone->getRawOffset();
    if (correction.offset) {
      corrected_raw_offset_ms = -(*correction.offset * 1000);
    }

    // If DST is not active for this zone, create a SimpleTimeZone without DST
    // rules. Otherwise, apply the corrected offset.
    if (!correction.dst_active) {
      new_zone = std::make_unique<icu::SimpleTimeZone>(
          corrected_raw_offset_ms, icu::UnicodeString::fromUTF8(tz_id.c_str()));
    } else {
      new_zone->setRawOffset(corrected_raw_offset_ms);
    }
  } else {
    // If no correction is found, extract names directly from the ICU data.
    std_name_ = ExtractZoneName(*new_zone, false, std::nullopt);
    if (new_zone->useDaylightTime()) {
      std::optional<UDate> last_dst_date = FindLastDateWithDst(*new_zone);
      dst_name_ = ExtractZoneName(*new_zone, true, last_dst_date);
    } else {
      dst_name_ = std_name_;
    }
  }
  return new_zone;
}

void TimeZoneState::EnsureTimeZoneIsCreated() {
  // The TZ environment variable is the primary source for the timezone.
  // If it's not set, fall back to the system's default timezone name.
  const char* timezone_name = getenv("TZ");
  if (!timezone_name) {
    timezone_name = SbTimeZoneGetName();
  }
  // std::string cannot be constructed from nullptr.
  std::string tz_to_process(timezone_name ? timezone_name : "");

  if (std::atomic_load(&current_zone_) &&
      cached_timezone_name_ == tz_to_process) {
    // timezone name is set but did not change.
    return;
  }

  std::unique_ptr<icu::TimeZone> new_zone;
  // An empty TZ variable explicitly means UTC.
  if (tz_to_process.empty()) {
    std_name_ = "UTC";
    dst_name_ = "UTC";
    new_zone = std::make_unique<icu::SimpleTimeZone>(
        0, icu::UnicodeString::fromUTF8("UTC"));
  } else {
    // First, try to parse the string as a POSIX-style TZ string. This format
    // includes rules for DST.
    auto timezone_data = tz::ParsePosixTz(tz_to_process);
    if (timezone_data) {
      std_name_ = timezone_data->std;
      dst_name_ = timezone_data->dst.value_or(timezone_data->std);
      new_zone = CreateIcuTimezoneFromParsedData(*timezone_data);
    } else {
      // If it's not a POSIX TZ string, treat it as an IANA timezone ID (e.g.,
      // "America/New_York").
      new_zone = CreateTimeZoneFromIanaId(tz_to_process);
    }
  }

  // Set the processed timezone as the default for the entire application.
  icu::TimeZone::setDefault(*new_zone);
  std::atomic_store(&current_zone_,
                    std::shared_ptr<const icu::TimeZone>(new_zone.release()));
  cached_timezone_name_ = tz_to_process;
}

}  // namespace time
}  // namespace libc
}  // namespace common
}  // namespace cobalt
