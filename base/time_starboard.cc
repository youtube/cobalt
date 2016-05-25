// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/time.h"

#include <unicode/ucal.h>
#include <unicode/uloc.h>
#include <unicode/ustring.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "starboard/time.h"

// Negative statuses are considered warnings, so this only fails if the status
// value is greater than zero.
#define UCHECK(status) DCHECK_GE(U_ZERO_ERROR, status) << __FUNCTION__ << ": "

namespace base {

namespace {
// Converts a Starboard time (microseconds since Windows epoch) into UDate
// (milliseconds since POSIX epoch as a double).
UDate SbTimeToUDate(SbTime sb_time) {
  return SbTimeToPosix(sb_time) /
         static_cast<UDate>(Time::kMicrosecondsPerMillisecond);
}

// Converts UDate into a Starboard time.
SbTime UDateToSbTime(UDate udate) {
  return SbTimeFromPosix(
      static_cast<int64_t>(udate * Time::kMicrosecondsPerMillisecond));
}

// Sets the time of the given calendar.
UErrorCode SetUCalendarTime(UCalendar* calendar, SbTime time) {
  UErrorCode status = U_ZERO_ERROR;
  UDate udate = SbTimeToUDate(time);
  ucal_setMillis(calendar, udate, &status);
  UCHECK(status) << "Failed to set UCalendar UDate to " << udate;
  return status;
}

SbTime GetUCalendarTime(UCalendar* calendar) {
  UErrorCode status = U_ZERO_ERROR;
  UDate udate = ucal_getMillis(calendar, &status);
  UCHECK(status) << "Failed to get UCalendar UDate.";
  return UDateToSbTime(udate);
}

// Creates a UTC UCalendar in the default locale.
UCalendar* CreateUCalendar(bool local) {
  UErrorCode status = U_ZERO_ERROR;
  const char* locale = uloc_getDefault();
  UCalendar* calendar;
  if (local) {
    calendar = ucal_open(NULL, -1, locale, UCAL_DEFAULT, &status);
  } else {
    U_STRING_DECL(time_zone, "Etc/GMT", 8);
    U_STRING_INIT(time_zone, "Etc/GMT", 8);
    calendar = ucal_open(time_zone, -1, locale, UCAL_DEFAULT, &status);
  }
  DCHECK(calendar) << "Unable to construct ICU calendar for " << locale;
  UCHECK(status) << "Unable to construct ICU calendar for " << locale;
  ucal_setMillis(calendar, 0, &status);
  return calendar;
}

// Creates a GMT UCalendar in the default locale with the given time.
UCalendar* CreateUCalendar(SbTime time, bool local) {
  UCalendar* calendar = CreateUCalendar(local);
  SetUCalendarTime(calendar, time);
  return calendar;
}

// Destroys the given UCalendar.
void DestroyUCalendar(UCalendar* calendar) {
  ucal_close(calendar);
}
}  // namespace

SbTime TimeDelta::ToSbTime() const {
  return InMicroseconds();
}

// static
const int64 Time::kTimeTToMicrosecondsOffset = -kSbTimeToPosixDelta;

// static
Time Time::Now() {
  return Time(SbTimeGetNow());
}

// static
Time Time::NowFromSystemTime() {
  return Now();
}

void Time::Explode(bool is_local, Exploded *exploded) const {
  UCalendar* calendar;
  calendar = CreateUCalendar(ToSbTime(), is_local);

  UErrorCode status = U_ZERO_ERROR;
  exploded->year = ucal_get(calendar, UCAL_YEAR, &status);
  UCHECK(status) << "Failed to get year.";

  status = U_ZERO_ERROR;
  exploded->month = ucal_get(calendar, UCAL_MONTH, &status) - UCAL_JANUARY + 1;
  UCHECK(status) << "Failed to get month.";

  status = U_ZERO_ERROR;
  exploded->day_of_week =
      (ucal_get(calendar, UCAL_DAY_OF_WEEK, &status) - UCAL_SUNDAY);
  UCHECK(status) << "Failed to get day of week.";

  status = U_ZERO_ERROR;
  exploded->day_of_month = ucal_get(calendar, UCAL_DAY_OF_MONTH, &status);
  UCHECK(status) << "Failed to get day of month.";

  status = U_ZERO_ERROR;
  exploded->hour = ucal_get(calendar, UCAL_HOUR_OF_DAY, &status);
  UCHECK(status) << "Failed to get hour of day.";

  status = U_ZERO_ERROR;
  exploded->minute = ucal_get(calendar, UCAL_MINUTE, &status);
  UCHECK(status) << "Failed to get minutes.";

  status = U_ZERO_ERROR;
  exploded->second = ucal_get(calendar, UCAL_SECOND, &status);
  UCHECK(status) << "Failed to get seconds.";

  status = U_ZERO_ERROR;
  exploded->millisecond = ucal_get(calendar, UCAL_MILLISECOND, &status);
  UCHECK(status) << "Failed to get milliseconds.";

  DestroyUCalendar(calendar);
}

// static
Time Time::FromExploded(bool is_local, const Exploded &exploded) {
  UCalendar* calendar = CreateUCalendar(is_local);
  UErrorCode status = U_ZERO_ERROR;
  ucal_setDateTime(calendar, exploded.year, exploded.month - 1 + UCAL_JANUARY,
                   exploded.day_of_month, exploded.hour, exploded.minute,
                   exploded.second, &status);
  UCHECK(status) << "Failed to set UCalendar DateTime.";
  ucal_set(calendar, UCAL_MILLISECOND, exploded.millisecond);

  SbTime sb_time = GetUCalendarTime(calendar);
  DestroyUCalendar(calendar);
  return Time::FromSbTime(sb_time);
}

// static
Time Time::FromSbTime(SbTime t) {
  return Time(t);
}

SbTime Time::ToSbTime() const {
  return us_;
}

// static
TimeTicks TimeTicks::Now() {
  return TimeTicks(SbTimeGetMonotonicNow());
}

// static
TimeTicks TimeTicks::HighResNow() {
  return Now();
}

// static
TimeTicks TimeTicks::NowFromSystemTraceTime() {
  return HighResNow();
}

}  // namespace base
