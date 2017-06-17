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

#include "base/basictypes.h"
#include "base/logging.h"
#include "starboard/client_porting/eztime/eztime.h"
#include "starboard/time.h"

namespace base {

namespace {
EzTimeZone GetTz(bool is_local) {
  return is_local ? kEzTimeZoneLocal : kEzTimeZoneUTC;
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
  EzTimeValue value = EzTimeValueFromSbTime(ToSbTime());
  EzTimeExploded ez_exploded;
  int millisecond;
  bool result = EzTimeValueExplode(&value, GetTz(is_local), &ez_exploded,
                                   &millisecond);
  DCHECK(result);

  exploded->year         = ez_exploded.tm_year + 1900;
  exploded->month        = ez_exploded.tm_mon + 1;
  exploded->day_of_week  = ez_exploded.tm_wday;
  exploded->day_of_month = ez_exploded.tm_mday;
  exploded->hour         = ez_exploded.tm_hour;
  exploded->minute       = ez_exploded.tm_min;
  exploded->second       = ez_exploded.tm_sec;
  exploded->millisecond  = millisecond;
}

// static
Time Time::FromExploded(bool is_local, const Exploded &exploded) {
  EzTimeExploded ez_exploded;
  ez_exploded.tm_sec   = exploded.second;
  ez_exploded.tm_min   = exploded.minute;
  ez_exploded.tm_hour  = exploded.hour;
  ez_exploded.tm_mday  = exploded.day_of_month;
  ez_exploded.tm_mon   = exploded.month - 1;
  ez_exploded.tm_year  = exploded.year - 1900;
  ez_exploded.tm_isdst = -1;
  EzTimeValue value = EzTimeValueImplode(&ez_exploded, exploded.millisecond,
                                         GetTz(is_local));
  return Time::FromSbTime(EzTimeValueToSbTime(&value));
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
TimeTicks TimeTicks::ThreadNow() {
#if SB_API_VERSION >= 3 && SB_HAS(TIME_THREAD_NOW)
  return TimeTicks(SbTimeGetMonotonicThreadNow());
#else
  return HighResNow();
#endif
}

// static
bool TimeTicks::HasThreadNow() {
#if SB_API_VERSION >= 3 && SB_HAS(TIME_THREAD_NOW)
  return true;
#else
  return false;
#endif
}

// static
TimeTicks TimeTicks::NowFromSystemTraceTime() {
  return HighResNow();
}

}  // namespace base
