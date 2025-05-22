// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include <array>
#include <cstddef>
#include <string>

#include "third_party/icu/source/common/unicode/udata.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/common/unicode/ustring.h"
#include "third_party/icu/source/i18n/unicode/ucal.h"

namespace {

const UChar kTimeZoneUTC[] = u"Etc/UTC";
const UChar* kTimeZoneLocal = nullptr;

// Converts time_usec to time_t. NOTE: This is LOSSY.
static time_t TimeUsecToTimeT(int64_t time_usec) {
  return time_usec >= 0 ? time_usec / 1'000'000
                        : (time_usec - 1'000'000 + 1) / 1'000'000;
}

// Converts time_usec to struct timeval.
static struct timeval TimeUsecToTimeval(int64_t time_usec) {
  time_t sec = TimeUsecToTimeT(time_usec);
  int64_t diff = time_usec - sec * 1'000'000;
  struct timeval tv = {sec, (int)diff};  // NOLINT(readability/casting)
  return tv;
}

// Converts struct timeval to time_usec.
static int64_t TimevalToTimeUsec(const struct timeval* tv) {
  return tv->tv_sec * 1'000'000 + tv->tv_usec;
}

// Converts from an ICU UDate to an time_usec. NOTE: This is LOSSY.
UDate TimeUsecToUDate(int64_t time_usec) {
  return static_cast<UDate>(time_usec >= 0 ? time_usec / 1000
                                           : (time_usec - 1000 + 1) / 1000);
}

// Explodes |value| to a time in the given |timezone|, placing the result in
// |out_exploded|, with the remainder milliseconds in |out_millisecond|, if not
// NULL. Returns whether the explosion was successful. NOTE: This is LOSSY.
bool timevalExplode(const struct timeval* value,
                    const UChar* zone_id,
                    struct tm* out_exploded,
                    int* out_milliseconds) {
  if (!value || !out_exploded) {
    return false;
  }
  UErrorCode status = U_ZERO_ERROR;

  // Always query the time using a gregorian calendar.  This is
  // implied in opengroup documentation for tm struct, even though it is not
  // specified.  E.g. in gmtime's documentation, it states that UTC time is
  // used, and in tm struct's documentation it is specified that year should
  // be an offset from 1900.

  // See:
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/gmtime.html
  UCalendar* calendar =
      ucal_open(zone_id, -1, uloc_getDefault(), UCAL_GREGORIAN, &status);
  if (!calendar) {
    return false;
  }

  int64_t sb_time = TimevalToTimeUsec(value);
  UDate udate = TimeUsecToUDate(sb_time);
  ucal_setMillis(calendar, udate, &status);
  out_exploded->tm_year = ucal_get(calendar, UCAL_YEAR, &status) - 1900;
  out_exploded->tm_mon = ucal_get(calendar, UCAL_MONTH, &status) - UCAL_JANUARY;
  out_exploded->tm_mday = ucal_get(calendar, UCAL_DATE, &status);
  out_exploded->tm_hour = ucal_get(calendar, UCAL_HOUR_OF_DAY, &status);
  out_exploded->tm_min = ucal_get(calendar, UCAL_MINUTE, &status);
  out_exploded->tm_sec = ucal_get(calendar, UCAL_SECOND, &status);
  out_exploded->tm_wday =
      ucal_get(calendar, UCAL_DAY_OF_WEEK, &status) - UCAL_SUNDAY;
  out_exploded->tm_yday = ucal_get(calendar, UCAL_DAY_OF_YEAR, &status) - 1;
  if (out_milliseconds) {
    *out_milliseconds = ucal_get(calendar, UCAL_MILLISECOND, &status);
  }
  out_exploded->tm_isdst = ucal_inDaylightTime(calendar, &status) ? 1 : 0;
  if (U_FAILURE(status)) {
    status = U_ZERO_ERROR;
    out_exploded->tm_isdst = -1;
  }

  ucal_close(calendar);
  return U_SUCCESS(status);
}

// Implodes |exploded| + |millisecond| as a time in |timezone|, returning the
// result as an struct timeval.
struct timeval timevalImplode(struct tm* exploded,
                              int millisecond,
                              const UChar* zone_id) {
  struct timeval zero_time = {};
  if (!exploded) {
    return zero_time;
  }
  UErrorCode status = U_ZERO_ERROR;

  // Always query the time using a gregorian calendar.  This is
  // implied in opengroup documentation for tm struct, even though it is not
  // specified.  E.g. in gmtime's documentation, it states that UTC time is
  // used, and in tm struct's documentation it is specified that year should
  // be an offset from 1900.

  // See:
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/gmtime.html
  UCalendar* calendar =
      ucal_open(zone_id, -1, uloc_getDefault(), UCAL_GREGORIAN, &status);
  if (!calendar) {
    return zero_time;
  }

  ucal_setMillis(calendar, 0, &status);  // Clear the calendar.
  ucal_setDateTime(calendar, exploded->tm_year + 1900,
                   exploded->tm_mon + UCAL_JANUARY, exploded->tm_mday,
                   exploded->tm_hour, exploded->tm_min, exploded->tm_sec,
                   &status);
  if (millisecond) {
    ucal_add(calendar, UCAL_MILLISECOND, millisecond, &status);
  }

  UDate udate = ucal_getMillis(calendar, &status);
  ucal_close(calendar);

  if (status <= U_ZERO_ERROR) {
    return TimeUsecToTimeval(udate * 1000LL);
  }

  return zero_time;
}

}  // namespace

struct tm* localtime_r(const time_t* in_time, struct tm* out_exploded) {
  struct timeval value = TimeUsecToTimeval(*in_time * 1'000'000);
  if (timevalExplode(&value, kTimeZoneLocal, out_exploded, NULL)) {
    return out_exploded;
  }

  return NULL;
}

struct tm* gmtime_r(const time_t* in_time, struct tm* out_exploded) {
  struct timeval value = TimeUsecToTimeval(*in_time * 1'000'000);
  if (timevalExplode(&value, kTimeZoneUTC, out_exploded, NULL)) {
    return out_exploded;
  }

  return NULL;
}

struct tm* gmtime(const time_t* in_time) {
  static thread_local struct tm gmtime;
  return gmtime_r(in_time, &gmtime);
}
struct tm* localtime(const time_t* in_time) {
  static thread_local struct tm localtime;
  return localtime_r(in_time, &localtime);
}

time_t mktime(struct tm* exploded) {
  struct timeval value = timevalImplode(exploded, 0, kTimeZoneLocal);
  return TimeUsecToTimeT(TimevalToTimeUsec(&value));
}

time_t timegm(struct tm* exploded) {
  struct timeval value = timevalImplode(exploded, 0, kTimeZoneUTC);
  return TimeUsecToTimeT(TimevalToTimeUsec(&value));
}
