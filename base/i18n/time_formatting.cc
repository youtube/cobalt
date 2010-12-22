// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/time.h"
#include "unicode/datefmt.h"

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

}  // namespace base
