// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic time formatting methods.  These methods use the current locale
// formatting for displaying the time.

#ifndef BASE_I18N_TIME_FORMATTING_H_
#define BASE_I18N_TIME_FORMATTING_H_
#pragma once

#include "base/string16.h"

namespace base {

class Time;

// Returns the time of day, e.g., "3:07 PM".
string16 TimeFormatTimeOfDay(const Time& time);

// Returns a shortened date, e.g. "Nov 7, 2007"
string16 TimeFormatShortDate(const Time& time);

// Returns a numeric date such as 12/13/52.
string16 TimeFormatShortDateNumeric(const Time& time);

// Formats a time in a friendly sentence format, e.g.
// "Monday, March 6, 2008 2:44:30 PM".
string16 TimeFormatShortDateAndTime(const Time& time);

// Formats a time in a friendly sentence format, e.g.
// "Monday, March 6, 2008 2:44:30 PM".
string16 TimeFormatFriendlyDateAndTime(const Time& time);

// Formats a time in a friendly sentence format, e.g.
// "Monday, March 6, 2008".
string16 TimeFormatFriendlyDate(const Time& time);

}  // namespace base

#endif  // BASE_I18N_TIME_FORMATTING_H_
