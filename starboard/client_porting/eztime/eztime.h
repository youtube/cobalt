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

#ifndef STARBOARD_CLIENT_PORTING_EZTIME_EZTIME_H_
#define STARBOARD_CLIENT_PORTING_EZTIME_EZTIME_H_

#if defined(STARBOARD)

#include "starboard/common/log.h"
#include "starboard/time.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Types ------------------------------------------------------------------

// A struct tm similar struct that can be used to get fields from an EzTimeT.
typedef struct EzTimeExploded {
  // Seconds after the minute [0, 61]
  int tm_sec;

  // Minutes after the hour [0, 59]
  int tm_min;

  // Hours since midnight [0, 23]
  int tm_hour;

  // Day of the month [1, 31]
  int tm_mday;

  // Months since January [0, 11]
  int tm_mon;

  // Years since 1900
  int tm_year;

  // Days since Sunday [0, 6]
  int tm_wday;

  // Days since January 1 [0, 365]
  int tm_yday;

  // Whether the time is in Daylight Savings Time.
  //   > 0 - DST is in effect
  //   = 0 - DST is not in effect
  //   < 0 - DST status is unknown
  int tm_isdst;
} EzTimeExploded;

// EzTimeT is a time_t-similar type that represents seconds since the POSIX
// epoch (midnight, Jan 1, 1970 UTC).
typedef int64_t EzTimeT;

// EzTimeValue is a struct timeval similar struct that represents a duration of
// seconds + microseconds.
typedef struct EzTimeValue {
  EzTimeT tv_sec;
  int32_t tv_usec;
} EzTimeValue;

// An enumeration of time zones that often need to be directly referenced.
typedef enum EzTimeZone {
  // The Pacific timezone. e.g. Where Mountain View is.
  kEzTimeZonePacific = 0,

  // The Universal Time Code timezone, which has no Daylight Savings and is
  // located at the Prime Meridian (in Greenwich).
  kEzTimeZoneUTC,

  // The local timezone, whatever that is based on SbTimeZone reporting.
  kEzTimeZoneLocal,

  // The number of EzTimeZones.
  kEzTimeZoneCount,
} EzTimeZone;

// --- Constants --------------------------------------------------------------

// One second in EzTimeT units.
#define kEzTimeTSecond 1

// One minute in EzTimeT units.
#define kEzTimeTMinute (kEzTimeTSecond * 60)

// One hour in EzTimeT units (microseconds).
#define kEzTimeTHour (kEzTimeTMinute * 60)

// One day in EzTimeT units (microseconds).
#define kEzTimeTDay (kEzTimeTHour * 24)

// The maximum value of an EzTimeT.
#define kEzTimeTMax (kSbInt64Max)

// A term that can be added to an EzTimeT to convert it into the number of
// microseconds since the Windows epoch.
#define kEzTimeTToWindowsDelta (SB_INT64_C(11644473600) * kEzTimeTSecond)

// --- Simple Conversion Functions --------------------------------------------

// Converts SbTime to EzTimeT. NOTE: This is LOSSY.
static SB_C_FORCE_INLINE EzTimeT EzTimeTFromSbTime(SbTime in_time) {
  return SbTimeNarrow(SbTimeToPosix(in_time), kSbTimeSecond);
}

// Converts EzTimeT to SbTime.
static SB_C_FORCE_INLINE SbTime EzTimeTToSbTime(EzTimeT in_time) {
  return SbTimeFromPosix(in_time * kSbTimeSecond);
}

// Converts SbTime to EzTimeValue.
static SB_C_FORCE_INLINE EzTimeValue EzTimeValueFromSbTime(SbTime in_time) {
  EzTimeT sec = EzTimeTFromSbTime(in_time);
  SbTime diff = in_time - EzTimeTToSbTime(sec);
  SB_DCHECK(diff >= INT_MIN);
  SB_DCHECK(diff <= INT_MAX);
  EzTimeValue value = {sec, (int)diff};  // Some compilers do not support
                                         // returning the initializer list
                                         // directly.
  return value;
}

// Converts EzTimeValue to SbTime.
static SB_C_FORCE_INLINE SbTime EzTimeValueToSbTime(const EzTimeValue* value) {
  return EzTimeTToSbTime(value->tv_sec) + value->tv_usec;
}

// Converts EzTimeT to EzTimeValue.
static SB_C_FORCE_INLINE EzTimeValue EzTimeTToEzTimeValue(EzTimeT in_time) {
  return EzTimeValueFromSbTime(EzTimeTToSbTime(in_time));
}

// Converts EzTimeValue to EzTimeT. NOTE: This is LOSSY.
static SB_C_FORCE_INLINE EzTimeT
EzTimeValueToEzTimeT(const EzTimeValue* value) {
  return EzTimeTFromSbTime(EzTimeValueToSbTime(value));
}

// --- Generalized Functions --------------------------------------------------

// Explodes |time_in| to a time in the given |timezone|, placing the result in
// |out_exploded|. Returns whether the explosion was successful.
bool EzTimeTExplode(const EzTimeT* SB_RESTRICT in_time,
                    EzTimeZone timezone,
                    EzTimeExploded* SB_RESTRICT out_exploded);

// Explodes |value| to a time in the given |timezone|, placing the result in
// |out_exploded|, with the remainder milliseconds in |out_millisecond|, if not
// NULL. Returns whether the explosion was successful. NOTE: This is LOSSY.
bool EzTimeValueExplode(const EzTimeValue* SB_RESTRICT value,
                        EzTimeZone timezone,
                        EzTimeExploded* SB_RESTRICT out_exploded,
                        int* SB_RESTRICT out_millisecond);

// Implodes |exploded| as a time in |timezone|, returning the result as an
// EzTimeT.
EzTimeT EzTimeTImplode(EzTimeExploded* SB_RESTRICT exploded,
                       EzTimeZone timezone);

// Implodes |exploded| + |millisecond| as a time in |timezone|, returning the
// result as an EzTimeValue.
EzTimeValue EzTimeValueImplode(EzTimeExploded* SB_RESTRICT exploded,
                               int millisecond,
                               EzTimeZone timezone);

// --- Replacement Functions --------------------------------------------------

// Gets the current time and places it in |out_tp|. |tzp| must always be
// NULL. Always returns 0. Meant to be a drop-in replacement for gettimeofday().
int EzTimeValueGetNow(EzTimeValue* SB_RESTRICT out_tp, void* SB_RESTRICT tzp);

// Gets the current time and places it in |out_now|, if specified, and also
// returns it. Meant to be a drop-in replacement for time().
EzTimeT EzTimeTGetNow(EzTimeT* out_now);

// Explodes |time_in| to a local time, placing the result in |out_exploded|, and
// returning |out_exploded|, or NULL in case of error. Meant to be a drop-in
// replacement for localtime_r().
EzTimeExploded* EzTimeTExplodeLocal(const EzTimeT* SB_RESTRICT in_time,
                                    EzTimeExploded* SB_RESTRICT out_exploded);

// Explodes |time_in| to a UTC time, placing the result in |out_exploded|, and
// returning |out_exploded|, or NULL in case of error. Meant to be a drop-in
// replacement for gmtime_r().
EzTimeExploded* EzTimeTExplodeUTC(const EzTimeT* SB_RESTRICT in_time,
                                  EzTimeExploded* SB_RESTRICT out_exploded);

// Implodes |exploded| as a local time, returning the result as an
// EzTimeT. Meant to be a drop-in replacement for mktime()/timelocal().
EzTimeT EzTimeTImplodeLocal(EzTimeExploded* SB_RESTRICT exploded);

// Implodes |exploded| as a UTC time, returning the result as an EzTimeT. Meant
// to be a drop-in replacement for timegm().
EzTimeT EzTimeTImplodeUTC(EzTimeExploded* SB_RESTRICT exploded);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD

#endif  // STARBOARD_CLIENT_PORTING_EZTIME_EZTIME_H_
