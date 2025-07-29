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

#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <string>

#include "cobalt/common/icu_init/init.h"
#include "cobalt/common/libc/time/timezone.h"
#include "third_party/icu/source/common/unicode/udata.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/common/unicode/ustring.h"
#include "third_party/icu/source/i18n/unicode/ucal.h"

// This file implements localtime_r(), gmtime_r(), gmtime(), localtime(),
// mktime(), and timegm() using ICU.
namespace {

const UChar kTimeZoneUTC[] = u"Etc/UTC";
const UChar* kTimeZoneLocal = nullptr;

// Converts time_usec to struct timeval.
static struct timeval TimeToTimeval(const time_t time_sec) {
  struct timeval tv = {time_sec, 0};  // NOLINT(readability/casting)
  return tv;
}

// Converts time_usec to time_t. NOTE: This is LOSSY.
static time_t UDateToTimeT(const UDate time_udate) {
  return time_udate >= 0 ? time_udate / 1'000
                         : (time_udate - 1'000 + 1) / 1'000;
}

// Converts time_usec to struct timeval.
static struct timeval UDateToTimeval(const UDate time_udate) {
  time_t sec = UDateToTimeT(time_udate);
  int64_t diff = time_udate * 1'000 - sec * 1'000'000;
  struct timeval tv = {sec, (int)diff};  // NOLINT(readability/casting)
  return tv;
}

// Converts struct timeval to time_usec.
static UDate TimevalToUDate(const struct timeval* tv) {
  return tv->tv_sec * 1'000 + tv->tv_usec / 1'000;
}

// Explodes |value| to a time in the given |zone_id|, placing the result in
// |out_exploded|, with the remainder milliseconds in |out_millisecond|, if not
// nullptr. Returns whether the explosion was successful. NOTE: This is LOSSY.
bool timevalExplode(const struct timeval* value,
                    const UChar* zone_id,
                    struct tm* out_exploded,
                    int* out_milliseconds) {
  if (!value || !out_exploded) {
    return false;
  }
  cobalt::common::icu_init::EnsureInitialized();

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

  UDate udate = TimevalToUDate(value);
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

  cobalt::common::libc::time::SetTmTimezoneFields(out_exploded, zone_id, udate);

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
  cobalt::common::icu_init::EnsureInitialized();

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

  // Update the exploded struct with the normalized values.
  exploded->tm_year = ucal_get(calendar, UCAL_YEAR, &status) - 1900;
  exploded->tm_mon = ucal_get(calendar, UCAL_MONTH, &status) - UCAL_JANUARY;
  exploded->tm_mday = ucal_get(calendar, UCAL_DATE, &status);
  exploded->tm_hour = ucal_get(calendar, UCAL_HOUR_OF_DAY, &status);
  exploded->tm_min = ucal_get(calendar, UCAL_MINUTE, &status);
  exploded->tm_sec = ucal_get(calendar, UCAL_SECOND, &status);
  exploded->tm_wday =
      ucal_get(calendar, UCAL_DAY_OF_WEEK, &status) - UCAL_SUNDAY;
  exploded->tm_yday = ucal_get(calendar, UCAL_DAY_OF_YEAR, &status) - 1;
  exploded->tm_isdst = ucal_inDaylightTime(calendar, &status) ? 1 : 0;

  UDate udate = ucal_getMillis(calendar, &status);
  ucal_close(calendar);

  if (status <= U_ZERO_ERROR) {
    return UDateToTimeval(udate);
  }

  return zero_time;
}

// The maximum time_t value that doesn't overflow 'struct tm'.
const time_t kMaxTimeValForStructTm =
    67767976233521999;  // Tue Dec 31 23:59:59 2147483647

const time_t kMinTimeValForStructTm =
    -67768040609712422;  // Thu Jan  1 00:00:00 -2147481748

}  // namespace

extern "C" {

struct tm* localtime_r(const time_t* in_time, struct tm* out_exploded) {
  if (!in_time) {
    errno = EINVAL;
    return nullptr;
  }
  if (*in_time < kMinTimeValForStructTm || *in_time > kMaxTimeValForStructTm) {
    errno = EOVERFLOW;
    return nullptr;
  }
  struct timeval value = TimeToTimeval(*in_time);
  if (timevalExplode(&value, kTimeZoneLocal, out_exploded, nullptr)) {
    return out_exploded;
  }

  return nullptr;
}

struct tm* gmtime_r(const time_t* in_time, struct tm* out_exploded) {
  if (!in_time) {
    errno = EINVAL;
    return nullptr;
  }
  if (*in_time < kMinTimeValForStructTm || *in_time > kMaxTimeValForStructTm) {
    errno = EOVERFLOW;
    return nullptr;
  }
  struct timeval value = TimeToTimeval(*in_time);
  if (timevalExplode(&value, kTimeZoneUTC, out_exploded, nullptr)) {
    return out_exploded;
  }

  return nullptr;
}

struct tm* gmtime(const time_t* in_time) {
  static thread_local struct tm gmtime;
  return gmtime_r(in_time, &gmtime);
}
struct tm* localtime(const time_t* in_time) {
  tzset();
  static thread_local struct tm localtime;
  return localtime_r(in_time, &localtime);
}

time_t mktime(struct tm* exploded) {
  tzset();
  struct timeval value = timevalImplode(exploded, 0, kTimeZoneLocal);
  return value.tv_sec;
}

time_t timegm(struct tm* exploded) {
  struct timeval value = timevalImplode(exploded, 0, kTimeZoneUTC);
  return value.tv_sec;
}

}  // extern "C"
