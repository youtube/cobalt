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
#include "starboard/time.h"

namespace base {

SbTime TimeDelta::ToSbTime() const {
  return InMicroseconds();
}

// static
Time Time::Now() {
  return Time(SbTimeGetNow());
}

// static
Time Time::NowFromSystemTime() {
  return Now();
}

void Time::Explode(bool is_local, Exploded *exploded) const {
  if (is_local) {
    NOTREACHED() << "Time::Explode with is_local = true is not safe on all "
                 << "platforms. Don't Do It!";
    return;
  }

  SbTimeExploded sb_exploded;
  SbTimeExplode(us_, &sb_exploded);
  exploded->year         = sb_exploded.year;
  exploded->month        = sb_exploded.month;
  exploded->day_of_week  = sb_exploded.day_of_week;
  exploded->day_of_month = sb_exploded.day_of_month;
  exploded->hour         = sb_exploded.hour;
  exploded->minute       = sb_exploded.minute;
  exploded->second       = sb_exploded.second;
  exploded->millisecond  = sb_exploded.millisecond;
}

// static
Time Time::FromExploded(bool is_local, const Exploded &exploded) {
  if (is_local) {
    NOTREACHED() << "Time::FromExploded with is_local = true is not safe on "
                 << "all platforms. Don't Do It!";
    return Time(0);
  }

  SbTimeExploded sb_exploded;
  sb_exploded.year         = exploded.year;
  sb_exploded.month        = exploded.month;
  sb_exploded.day_of_week  = exploded.day_of_week;
  sb_exploded.day_of_month = exploded.day_of_month;
  sb_exploded.hour         = exploded.hour;
  sb_exploded.minute       = exploded.minute;
  sb_exploded.second       = exploded.second;
  sb_exploded.millisecond  = exploded.millisecond;

  return Time(SbTimeImplode(&sb_exploded));
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
