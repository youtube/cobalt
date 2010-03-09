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

std::wstring TimeFormat(const icu::DateFormat* formatter,
                        const Time& time) {
  DCHECK(formatter);
  icu::UnicodeString date_string;

  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000), date_string);
  std::wstring output;
  bool success = UTF16ToWide(date_string.getBuffer(), date_string.length(),
      &output);
  DCHECK(success);
  return output;
}

}  // namespace

namespace base {

std::wstring TimeFormatTimeOfDay(const Time& time) {
  // We can omit the locale parameter because the default should match
  // Chrome's application locale.
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createTimeInstance(icu::DateFormat::kShort));
  return TimeFormat(formatter.get(), time);
}

std::wstring TimeFormatShortDate(const Time& time) {
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kMedium));
  return TimeFormat(formatter.get(), time);
}

std::wstring TimeFormatShortDateNumeric(const Time& time) {
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kShort));
  return TimeFormat(formatter.get(), time);
}

std::wstring TimeFormatShortDateAndTime(const Time& time) {
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateTimeInstance(icu::DateFormat::kShort));
  return TimeFormat(formatter.get(), time);
}

std::wstring TimeFormatFriendlyDateAndTime(const Time& time) {
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateTimeInstance(icu::DateFormat::kFull));
  return TimeFormat(formatter.get(), time);
}

std::wstring TimeFormatFriendlyDate(const Time& time) {
  scoped_ptr<icu::DateFormat> formatter(icu::DateFormat::createDateInstance(
      icu::DateFormat::kFull));
  return TimeFormat(formatter.get(), time);
}

}  // namespace base
