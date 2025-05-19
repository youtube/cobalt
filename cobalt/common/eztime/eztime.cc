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

#include "cobalt/common/eztime/eztime.h"

#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>

#include <array>
#include <cstddef>
#include <string>

#include "third_party/icu/source/common/unicode/udata.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/common/unicode/ustring.h"
#include "third_party/icu/source/i18n/unicode/ucal.h"

namespace {

// The number of characters to reserve for each time zone name.
const int kMaxTimeZoneSize = 32;

// Since ICU APIs take timezones by Unicode name strings, this is a cache of
// commonly used timezones that can be passed into ICU functions that take
// them.
UChar g_timezones[kEzTimeZoneCount][kMaxTimeZoneSize];

// Once control for initializing eztime static data.
pthread_once_t g_eztime_initialization_once = PTHREAD_ONCE_INIT;

// The timezone names in ASCII (UTF8-compatible) literals. This must match the
// order of the EzTimeZone enum.
constexpr std::array<const char*, kEzTimeZoneCount> kTimeZoneNames = {{
    "America/Los_Angeles",
    "Etc/GMT",
    "",
}};

// Standard compile-time assertion
static_assert(kTimeZoneNames.size() == kEzTimeZoneCount,
              "The number of kTimeZoneNames must match kEzTimeZoneCount.");

// Initializes a single timezone in the ICU timezone lookup table.
void InitializeTimeZone(EzTimeZone ez_timezone) {
  UErrorCode status = U_ZERO_ERROR;
  UChar* timezone_name = g_timezones[ez_timezone];
  const char* timezone_name_utf8 = kTimeZoneNames[ez_timezone];
  if (!timezone_name_utf8) {
    timezone_name[0] = 0;
    return;
  }

  u_strFromUTF8(timezone_name, kMaxTimeZoneSize, NULL, timezone_name_utf8, -1,
                &status);
}

// Initializes ICU and TimeZones so the rest of the functions will work.
// Should only be called once.
void Initialize() {
  // Initialize |g_timezones| table.
  for (int timezone = 0; timezone < kEzTimeZoneCount; ++timezone) {
    InitializeTimeZone(static_cast<EzTimeZone>(timezone));
  }
}

// Converts from an SbTime to an ICU UDate (non-fractional milliseconds since
// POSIX epoch, as a double).
int64_t UDateToSbTime(UDate udate) {
  return static_cast<int64_t>(udate * 1000LL);
}

// Converts from an ICU UDate to an SbTime. NOTE: This is LOSSY.
UDate SbTimeToUDate(int64_t posix_time) {
  return static_cast<UDate>(posix_time >= 0 ? posix_time / 1000
                                            : (posix_time - 1000 + 1) / 1000);
}

// Gets the cached TimeZone ID from |g_timezones| for the given EzTimeZone
// |ez_timezone|.
const UChar* GetTimeZoneId(EzTimeZone ez_timezone) {
  pthread_once(&g_eztime_initialization_once, &Initialize);
  const UChar* timezone_id = g_timezones[ez_timezone];
  if (timezone_id[0] == 0) {
    return NULL;
  }

  return timezone_id;
}

// Converts EzTimeValue to EzTimeT. NOTE: This is LOSSY.
static EzTimeT EzTimeValueToEzTimeT(const EzTimeValue* value) {
  return EzTimeTFromSbTime(EzTimeValueToSbTime(value));
}

}  // namespace

bool EzTimeTExplode(const EzTimeT* in_time,
                    EzTimeZone ez_timezone,
                    EzTimeExploded* out_exploded) {
  if (!in_time) {
    return false;
  }
  EzTimeValue value = EzTimeTToEzTimeValue(*in_time);
  return EzTimeValueExplode(&value, ez_timezone, out_exploded, NULL);
}

bool EzTimeValueExplode(const EzTimeValue* value,
                        EzTimeZone ez_timezone,
                        EzTimeExploded* out_exploded,
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
  UCalendar* calendar = ucal_open(GetTimeZoneId(ez_timezone), -1,
                                  uloc_getDefault(), UCAL_GREGORIAN, &status);
  if (!calendar) {
    return false;
  }

  int64_t sb_time = EzTimeValueToSbTime(value);
  UDate udate = SbTimeToUDate(sb_time);
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

EzTimeT EzTimeTImplode(EzTimeExploded* exploded, EzTimeZone ez_timezone) {
  EzTimeValue value = EzTimeValueImplode(exploded, 0, ez_timezone);
  return EzTimeValueToEzTimeT(&value);
}

EzTimeValue EzTimeValueImplode(EzTimeExploded* exploded,
                               int millisecond,
                               EzTimeZone ez_timezone) {
  EzTimeValue zero_time = {};
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
  UCalendar* calendar = ucal_open(GetTimeZoneId(ez_timezone), -1,
                                  uloc_getDefault(), UCAL_GREGORIAN, &status);
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
    return EzTimeValueFromSbTime(UDateToSbTime(udate));
  }

  return zero_time;
}

EzTimeT EzTimeTGetNow(EzTimeT* out_now) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    return 0;
  }
  EzTimeT result = tv.tv_sec;
  if (out_now) {
    *out_now = result;
  }
  return result;
}

EzTimeExploded* EzTimeTExplodeLocal(const EzTimeT* in_time,
                                    EzTimeExploded* out_exploded) {
  if (EzTimeTExplode(in_time, kEzTimeZoneLocal, out_exploded)) {
    return out_exploded;
  }

  return NULL;
}

EzTimeExploded* EzTimeTExplodeUTC(const EzTimeT* in_time,
                                  EzTimeExploded* out_exploded) {
  if (EzTimeTExplode(in_time, kEzTimeZoneUTC, out_exploded)) {
    return out_exploded;
  }

  return NULL;
}

EzTimeT EzTimeTImplodeLocal(EzTimeExploded* exploded) {
  return EzTimeTImplode(exploded, kEzTimeZoneLocal);
}

EzTimeT EzTimeTImplodeUTC(EzTimeExploded* exploded) {
  return EzTimeTImplode(exploded, kEzTimeZoneUTC);
}
