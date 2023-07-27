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

#include "starboard/client_porting/eztime/eztime.h"

#include <string>

#include "starboard/client_porting/icu_init/icu_init.h"
#include "starboard/common/log.h"
#include "starboard/once.h"
#include "starboard/system.h"
#include "starboard/time.h"

#include "third_party/icu/source/common/unicode/udata.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/common/unicode/ustring.h"
#include "third_party/icu/source/i18n/unicode/ucal.h"

namespace {

// The number of characters to reserve for each time zone name.
const int kMaxTimeZoneSize = 32;

// Since ICU APIs take timezones by Unicode name strings, this is a cache of
// commonly used timezones that can be passed into ICU functions that take them.
UChar g_timezones[kEzTimeZoneCount][kMaxTimeZoneSize];

// Once control for initializing eztime static data.
SbOnceControl g_initialization_once = SB_ONCE_INITIALIZER;

// The timezone names in ASCII (UTF8-compatible) literals. This must match the
// order of the EzTimeZone enum.
const char* kTimeZoneNames[] = {
    "America/Los_Angeles", "Etc/GMT", "",
};

SB_COMPILE_ASSERT(SB_ARRAY_SIZE(kTimeZoneNames) == kEzTimeZoneCount,
                  names_for_each_time_zone);

// Initializes a single timezone in the ICU timezone lookup table.
void InitializeTimeZone(EzTimeZone timezone) {
  UErrorCode status = U_ZERO_ERROR;
  UChar* timezone_name = g_timezones[timezone];
  const char* timezone_name_utf8 = kTimeZoneNames[timezone];
  if (!timezone_name_utf8) {
    timezone_name[0] = 0;
    return;
  }

  u_strFromUTF8(timezone_name, kMaxTimeZoneSize, NULL, timezone_name_utf8, -1,
                &status);
  SB_DCHECK(U_SUCCESS(status));
}

// Initializes ICU and TimeZones so the rest of the functions will work. Should
// only be called once.
void Initialize() {
  SbIcuInit();

  // Initialize |g_timezones| table.
  for (int timezone = 0; timezone < kEzTimeZoneCount; ++timezone) {
    InitializeTimeZone(static_cast<EzTimeZone>(timezone));
  }
}

// Converts from an SbTime to an ICU UDate (non-fractional milliseconds since
// POSIX epoch, as a double).
SbTime UDateToSbTime(UDate udate) {
  return SbTimeFromPosix(static_cast<int64_t>(udate * kSbTimeMillisecond));
}

// Converts from an ICU UDate to an SbTime. NOTE: This is LOSSY.
UDate SbTimeToUDate(SbTime sb_time) {
  return static_cast<UDate>(
      SbTimeNarrow(SbTimeToPosix(sb_time), kSbTimeMillisecond));
}

// Gets the cached TimeZone ID from |g_timezones| for the given EzTimeZone
// |timezone|.
const UChar* GetTimeZoneId(EzTimeZone timezone) {
  SbOnce(&g_initialization_once, &Initialize);
  const UChar* timezone_id = g_timezones[timezone];
  if (timezone_id[0] == 0) {
    return NULL;
  }

  return timezone_id;
}

}  // namespace

bool EzTimeTExplode(const EzTimeT* SB_RESTRICT in_time,
                    EzTimeZone timezone,
                    EzTimeExploded* SB_RESTRICT out_exploded) {
  SB_DCHECK(in_time);
  EzTimeValue value = EzTimeTToEzTimeValue(*in_time);
  return EzTimeValueExplode(&value, timezone, out_exploded, NULL);
}

bool EzTimeValueExplode(const EzTimeValue* SB_RESTRICT value,
                        EzTimeZone timezone,
                        EzTimeExploded* SB_RESTRICT out_exploded,
                        int* SB_RESTRICT out_milliseconds) {
  SB_DCHECK(value);
  SB_DCHECK(out_exploded);
  UErrorCode status = U_ZERO_ERROR;

  // Always query the time using a gregorian calendar.  This is
  // implied in opengroup documentation for tm struct, even though it is not
  // specified.  E.g. in gmtime's documentation, it states that UTC time is
  // used, and in tm struct's documentation it is specified that year should
  // be an offset from 1900.

  // See:
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/gmtime.html
  UCalendar* calendar = ucal_open(GetTimeZoneId(timezone), -1,
                                  uloc_getDefault(), UCAL_GREGORIAN, &status);
  if (!calendar) {
    return false;
  }

  SbTime sb_time = EzTimeValueToSbTime(value);
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

EzTimeT EzTimeTImplode(EzTimeExploded* SB_RESTRICT exploded,
                       EzTimeZone timezone) {
  EzTimeValue value = EzTimeValueImplode(exploded, 0, timezone);
  return EzTimeValueToEzTimeT(&value);
}

EzTimeValue EzTimeValueImplode(EzTimeExploded* SB_RESTRICT exploded,
                               int millisecond,
                               EzTimeZone timezone) {
  SB_DCHECK(exploded);
  UErrorCode status = U_ZERO_ERROR;

  // Always query the time using a gregorian calendar.  This is
  // implied in opengroup documentation for tm struct, even though it is not
  // specified.  E.g. in gmtime's documentation, it states that UTC time is
  // used, and in tm struct's documentation it is specified that year should
  // be an offset from 1900.

  // See:
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/gmtime.html
  UCalendar* calendar = ucal_open(GetTimeZoneId(timezone), -1,
                                  uloc_getDefault(), UCAL_GREGORIAN, &status);
  if (!calendar) {
    EzTimeValue zero_time = {};
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

  EzTimeValue zero_time = {};
  return zero_time;
}

int EzTimeValueGetNow(EzTimeValue* out_tp, void* tzp) {
  SB_DCHECK(tzp == NULL);
  SB_DCHECK(out_tp != NULL);
  *out_tp = EzTimeValueFromSbTime(SbTimeGetNow());
  return 0;
}

EzTimeT EzTimeTGetNow(EzTimeT* out_now) {
  EzTimeT result = EzTimeTFromSbTime(SbTimeGetNow());
  if (out_now) {
    *out_now = result;
  }
  return result;
}

EzTimeExploded* EzTimeTExplodeLocal(const EzTimeT* SB_RESTRICT in_time,
                                    EzTimeExploded* SB_RESTRICT out_exploded) {
  if (EzTimeTExplode(in_time, kEzTimeZoneLocal, out_exploded)) {
    return out_exploded;
  }

  return NULL;
}

EzTimeExploded* EzTimeTExplodeUTC(const EzTimeT* SB_RESTRICT in_time,
                                  EzTimeExploded* SB_RESTRICT out_exploded) {
  if (EzTimeTExplode(in_time, kEzTimeZoneUTC, out_exploded)) {
    return out_exploded;
  }

  return NULL;
}

EzTimeT EzTimeTImplodeLocal(EzTimeExploded* SB_RESTRICT exploded) {
  return EzTimeTImplode(exploded, kEzTimeZoneLocal);
}

EzTimeT EzTimeTImplodeUTC(EzTimeExploded* SB_RESTRICT exploded) {
  return EzTimeTImplode(exploded, kEzTimeZoneUTC);
}
