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

#include "SkString.h"
#include "SkTime.h"
#include "SkTypes.h"

#include "base/time.h"

#include "starboard/time.h"

// Taken from SkTime.cpp.
void SkTime::DateTime::toISO8601(SkString* dst) const {
  if (dst) {
    int timeZoneMinutes = SkToInt(fTimeZoneMinutes);
    char timezoneSign = timeZoneMinutes >= 0 ? '+' : '-';
    int timeZoneHours = SkTAbs(timeZoneMinutes) / 60;
    timeZoneMinutes = SkTAbs(timeZoneMinutes) % 60;
    dst->printf("%04u-%02u-%02uT%02u:%02u:%02u%c%02d:%02d",
                static_cast<unsigned>(fYear), static_cast<unsigned>(fMonth),
                static_cast<unsigned>(fDay), static_cast<unsigned>(fHour),
                static_cast<unsigned>(fMinute), static_cast<unsigned>(fSecond),
                timezoneSign, timeZoneHours, timeZoneMinutes);
  }
}

void SkTime::GetDateTime(DateTime* dt) {
  if (!dt) {
    return;
  }

  base::Time time = base::Time::Now();
  base::Time::Exploded exploded;
  base::Time::Now().UTCExplode(&exploded);

  dt->fYear = exploded.year;
  dt->fMonth = SkToU8(exploded.month);
  dt->fDayOfWeek = SkToU8(exploded.day_of_week);
  dt->fDay = SkToU8(exploded.day_of_month);
  dt->fHour = SkToU8(exploded.hour);
  dt->fMinute = SkToU8(exploded.minute);
  dt->fSecond = SkToU8(exploded.second);
}

double SkTime::GetNSecs() {
  return SbTimeGetMonotonicNow() * kSbTimeNanosecondsPerMicrosecond;
}
