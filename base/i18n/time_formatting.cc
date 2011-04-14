// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/time.h"
#include "unicode/datefmt.h"
#include "unicode/dtptngen.h"
#include "unicode/smpdtfmt.h"

using base::Time;

namespace {

string16 TimeFormat(const icu::DateFormat* formatter,
                    const Time& time) {
  DCHECK(formatter);
  icu::UnicodeString date_string;

  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000), date_string);
  return string16(date_string.getBuffer(),
                  static_cast<size_t>(date_string.length()));
}

}  // namespace

namespace base {

string16 TimeFormatTimeOfDay(const Time& time) {
  // We can omit the locale parameter because the default should match
  // Chrome's application locale.
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createTimeInstance(icu::DateFormat::kShort));
  return TimeFormat(formatter.get(), time);
}

string16 TimeFormatTimeOfDayWithHourClockType(const Time& time,
                                              HourClockType type) {
  // Just redirect to the normal function if the default type matches the
  // given type.
  HourClockType default_type = GetHourClockType();
  if (default_type == type) {
    return TimeFormatTimeOfDay(time);
  }

  // Generate a locale-dependent format pattern. The generator will take
  // care of locale-dependent formatting issues like which separator to
  // use (some locales use '.' instead of ':'), and where to put the am/pm
  // marker.
  UErrorCode status = U_ZERO_ERROR;
  icu::DateTimePatternGenerator *generator =
      icu::DateTimePatternGenerator::createInstance(status);
  CHECK(U_SUCCESS(status));
  const char* base_pattern = (type == k12HourClock ? "ahm" : "Hm");
  icu::UnicodeString generated_pattern =
      generator->getBestPattern(icu::UnicodeString(base_pattern), status);
  CHECK(U_SUCCESS(status));

  // Then, format the time using the generated pattern.
  icu::SimpleDateFormat formatter(generated_pattern, status);
  CHECK(U_SUCCESS(status));
  return TimeFormat(&formatter, time);
}

string16 TimeFormatShortDate(const Time& time) {
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kMedium));
  return TimeFormat(formatter.get(), time);
}

string16 TimeFormatShortDateNumeric(const Time& time) {
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kShort));
  return TimeFormat(formatter.get(), time);
}

string16 TimeFormatShortDateAndTime(const Time& time) {
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateTimeInstance(icu::DateFormat::kShort));
  return TimeFormat(formatter.get(), time);
}

string16 TimeFormatFriendlyDateAndTime(const Time& time) {
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateTimeInstance(icu::DateFormat::kFull));
  return TimeFormat(formatter.get(), time);
}

string16 TimeFormatFriendlyDate(const Time& time) {
  scoped_ptr<icu::DateFormat> formatter(icu::DateFormat::createDateInstance(
      icu::DateFormat::kFull));
  return TimeFormat(formatter.get(), time);
}

HourClockType GetHourClockType() {
  // TODO(satorux,jshin): Rework this with ures_getByKeyWithFallback()
  // once it becomes public. The short time format can be found at
  // "calendar/gregorian/DateTimePatterns/3" in the resources.
  scoped_ptr<icu::SimpleDateFormat> formatter(
      static_cast<icu::SimpleDateFormat*>(
          icu::DateFormat::createTimeInstance(icu::DateFormat::kShort)));
  // Retrieve the short time format.
  icu::UnicodeString pattern_unicode;
  formatter->toPattern(pattern_unicode);

  // Determine what hour clock type the current locale uses, by checking
  // "a" (am/pm marker) in the short time format. This is reliable as "a"
  // is used by all of 12-hour clock formats, but not any of 24-hour clock
  // formats, as shown below.
  //
  // % grep -A4 DateTimePatterns third_party/icu/source/data/locales/*.txt |
  //   grep -B1 -- -- |grep -v -- '--' |
  //   perl -nle 'print $1 if /^\S+\s+"(.*)"/' |sort -u
  //
  // H.mm
  // H:mm
  // HH.mm
  // HH:mm
  // a h:mm
  // ah:mm
  // ahh:mm
  // h-mm a
  // h:mm a
  // hh:mm a
  //
  // See http://userguide.icu-project.org/formatparse/datetime for details
  // about the date/time format syntax.
  if (pattern_unicode.indexOf('a') == -1) {
    return k24HourClock;
  } else {
    return k12HourClock;
  }
}

}  // namespace base
