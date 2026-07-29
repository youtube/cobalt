// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <time.h>

#include "cobalt/common/libc/time/icu_time_support.h"
#include "starboard/common/log.h"

extern "C" {
// Global C variables.
long timezone = 0;
int daylight = 0;
char* tzname[2] = {nullptr, nullptr};

void tzset(void) {
  // A thread-local flag to detect re-entrant calls to tzset().
  static thread_local bool in_tzset = false;
  if (in_tzset) {
    SB_NOTREACHED() << "tzset() re-entrancy detected.";
    return;
  }

  // Scoped guard to set the flag and reset it on exit.
  struct TzsetGuard {
    bool& in_tzset_flag;
    TzsetGuard(bool& flag) : in_tzset_flag(flag) { in_tzset_flag = true; }
    ~TzsetGuard() { in_tzset_flag = false; }
  };
  TzsetGuard guard(in_tzset);

  auto* time_support =
      cobalt::common::libc::time::IcuTimeSupport::GetInstance();
  time_support->GetPosixTimezoneGlobals(timezone, daylight, tzname);
}
}  // extern "C"
