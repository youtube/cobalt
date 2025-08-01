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
#include <memory>
#include "unicode/calendar.h"
#include "unicode/gregocal.h"
#include "unicode/timezone.h"
#include "unicode/unistr.h"

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
                    const icu::TimeZone* zone,
                    struct tm* out_exploded,
                    int* out_milliseconds) {
  if (!value || !out_exploded) {
    return false;
  }
  cobalt::common::icu_init::EnsureInitialized();

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> calendar(
      new icu::GregorianCalendar(zone ? zone->clone() : nullptr, status));
  if (!calendar || U_FAILURE(status)) {
    return false;
  }

  UDate udate = TimevalToUDate(value);
  calendar->setTime(udate, status);
  out_exploded->tm_year = calendar->get(UCAL_YEAR, status) - 1900;
  out_exploded->tm_mon = calendar->get(UCAL_MONTH, status);
  out_exploded->tm_mday = calendar->get(UCAL_DATE, status);
  out_exploded->tm_hour = calendar->get(UCAL_HOUR_OF_DAY, status);
  out_exploded->tm_min = calendar->get(UCAL_MINUTE, status);
  out_exploded->tm_sec = calendar->get(UCAL_SECOND, status);
  out_exploded->tm_wday = calendar->get(UCAL_DAY_OF_WEEK, status) - UCAL_SUNDAY;
  out_exploded->tm_yday = calendar->get(UCAL_DAY_OF_YEAR, status) - 1;
  if (out_milliseconds) {
    *out_milliseconds = calendar->get(UCAL_MILLISECOND, status);
  }
  out_exploded->tm_isdst = calendar->inDaylightTime(status) ? 1 : 0;
  if (U_FAILURE(status)) {
    status = U_ZERO_ERROR;
    out_exploded->tm_isdst = -1;
  }

  cobalt::common::libc::time::SetTmTimezoneFields(out_exploded, *zone, udate);

  return U_SUCCESS(status);
}

// Implodes |exploded| + |millisecond| as a time in |timezone|, returning the
// result as an struct timeval.
struct timeval timevalImplode(struct tm* exploded,
                              int millisecond,
                              const icu::TimeZone* zone) {
  struct timeval zero_time = {};
  if (!exploded) {
    return zero_time;
  }
  cobalt::common::icu_init::EnsureInitialized();

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> calendar(
      new icu::GregorianCalendar(zone ? zone->clone() : nullptr, status));
  if (!calendar || U_FAILURE(status)) {
    return zero_time;
  }

  calendar->clear();
  calendar->set(exploded->tm_year + 1900, exploded->tm_mon, exploded->tm_mday,
                exploded->tm_hour, exploded->tm_min, exploded->tm_sec);
  if (millisecond) {
    calendar->add(UCAL_MILLISECOND, millisecond, status);
  }

  // Update the exploded struct with the normalized values.
  exploded->tm_year = calendar->get(UCAL_YEAR, status) - 1900;
  exploded->tm_mon = calendar->get(UCAL_MONTH, status);
  exploded->tm_mday = calendar->get(UCAL_DATE, status);
  exploded->tm_hour = calendar->get(UCAL_HOUR_OF_DAY, status);
  exploded->tm_min = calendar->get(UCAL_MINUTE, status);
  exploded->tm_sec = calendar->get(UCAL_SECOND, status);
  exploded->tm_wday = calendar->get(UCAL_DAY_OF_WEEK, status) - UCAL_SUNDAY;
  exploded->tm_yday = calendar->get(UCAL_DAY_OF_YEAR, status) - 1;
  exploded->tm_isdst = calendar->inDaylightTime(status) ? 1 : 0;

  UDate udate = calendar->getTime(status);

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

  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());

  struct timeval value = TimeToTimeval(*in_time);
  if (timevalExplode(&value, zone.get(), out_exploded, nullptr)) {
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
  if (timevalExplode(&value, icu::TimeZone::getGMT(), out_exploded, nullptr)) {
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
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
  struct timeval value = timevalImplode(exploded, 0, zone.get());
  return value.tv_sec;
}

time_t timegm(struct tm* exploded) {
  struct timeval value = timevalImplode(exploded, 0, icu::TimeZone::getGMT());
  return value.tv_sec;
}

}  // extern "C"