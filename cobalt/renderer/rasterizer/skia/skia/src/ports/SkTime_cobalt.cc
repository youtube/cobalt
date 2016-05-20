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

#include "SkTime.h"

#include "base/time.h"

// static
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

// static
SkMSec SkTime::GetMSecs() {
  return static_cast<SkMSec>(
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds());
}
