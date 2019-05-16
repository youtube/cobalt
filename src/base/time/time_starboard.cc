// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "base/time/time.h"

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
bool Time::FromExploded(bool is_local, const Exploded& exploded, Time* time) {
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
  base::Time converted_time(Time::FromSbTime(EzTimeValueToSbTime(&value)));

  // If |exploded.day_of_month| is set to 31 on a 28-30 day month, it will
  // return the first day of the next month. Thus round-trip the time and
  // compare the initial |exploded| with |utc_to_exploded| time.
  base::Time::Exploded to_exploded;
  if (!is_local) {
    converted_time.UTCExplode(&to_exploded);
  } else {
    converted_time.LocalExplode(&to_exploded);
  }

  if (ExplodedMostlyEquals(to_exploded, exploded)) {
    *time = converted_time;
    return true;
  }

  *time = Time(0);
  return false;
}

// static
Time Time::FromSbTime(SbTime t) {
  return Time(t);
}

SbTime Time::ToSbTime() const {
  return us_;
}

// static
bool TimeTicks::IsHighResolution() {
  return true;
}

}  // namespace base
