// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "base/test/time_helpers.h"

#include <unicode/ucal.h>
#include <unicode/uloc.h>
#include <unicode/ustring.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "starboard/types.h"

// Negative statuses are considered warnings, so this only fails if the status
// value is greater than zero.
#define UCHECK(status) DCHECK_GE(U_ZERO_ERROR, status) << __FUNCTION__ << ": "

namespace base {
namespace test {
namespace time_helpers {

namespace {
base::Time UDateToTime(UDate udate) {
  return base::Time::UnixEpoch() +
         base::TimeDelta::FromMicroseconds(static_cast<int64_t>(
             udate * base::Time::kMicrosecondsPerMillisecond));
}

const char* ToMonthString(int month) {
  switch (month) {
    case 1:
      return "Jan";
    case 2:
      return "Feb";
    case 3:
      return "Mar";
    case 4:
      return "Apr";
    case 5:
      return "May";
    case 6:
      return "Jun";
    case 7:
      return "Jul";
    case 8:
      return "Aug";
    case 9:
      return "Sep";
    case 10:
      return "Oct";
    case 11:
      return "Nov";
    case 12:
      return "Dec";
  }

  return "UNK";
}
}  // namespace

base::Time FieldsToTime(TimeZone timezone,
                        int year,
                        int month,
                        int date,
                        int hour,
                        int minute,
                        int second) {
  UErrorCode status = U_ZERO_ERROR;
  UChar timezone_buffer[32] = {0};
  UChar* timezone_uname = NULL;
  switch (timezone) {
    case kTimeZonePacific:
      u_strFromUTF8(timezone_buffer, arraysize(timezone_buffer), NULL,
                    "America/Los_Angeles", -1, &status);
      timezone_uname = timezone_buffer;
      break;
    case kTimeZoneGMT:
      u_strFromUTF8(timezone_buffer, arraysize(timezone_buffer), NULL,
                    "Etc/GMT", -1, &status);
      timezone_uname = timezone_buffer;
    case kTimeZoneLocal:
      // Leave NULL.
      break;
  }

  UCHECK(status);
  UCalendar* calendar =
      ucal_open(timezone_uname, -1, uloc_getDefault(), UCAL_DEFAULT, &status);
  DCHECK(calendar);
  UCHECK(status);
  ucal_setMillis(calendar, 0, &status);  // Clear the calendar.
  UCHECK(status);
  ucal_setDateTime(calendar, year, month - 1 + UCAL_JANUARY, date, hour, minute,
                   second, &status);
  UCHECK(status);
  UDate udate = ucal_getMillis(calendar, &status);
  UCHECK(status);
  ucal_close(calendar);
  return UDateToTime(udate);
}

base::Time TestDateToTime(TimeZone timezone) {
  return FieldsToTime(timezone,
                      2007,  // year
                      10,    // month
                      15,    // date
                      12,    // hour
                      45,    // minute
                      0);    // second
}

std::string TimeFormatUTC(base::Time time) {
  Time::Exploded exploded;
  time.UTCExplode(&exploded);
  return base::StringPrintf("%s %02d %02d:%02d:%02d %04d",
                            ToMonthString(exploded.month),
                            exploded.day_of_month, exploded.hour,
                            exploded.minute, exploded.second, exploded.year);
}

std::string TimeFormatLocal(base::Time time) {
  Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return base::StringPrintf("%s %02d %02d:%02d:%02d %04d",
                            ToMonthString(exploded.month),
                            exploded.day_of_month, exploded.hour,
                            exploded.minute, exploded.second, exploded.year);
}

}  // namespace time_helpers
}  // namespace test
}  // namespace base
