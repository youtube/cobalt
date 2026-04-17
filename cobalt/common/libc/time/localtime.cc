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

extern "C" {
struct tm* localtime_r(const time_t* time, struct tm* result) {
  auto* time_support =
      cobalt::common::libc::time::IcuTimeSupport::GetInstance();
  if (time_support->ExplodeLocalTime(time, result)) {
    return result;
  }
  return nullptr;
}

struct tm* localtime(const time_t* time) {
  // Per POSIX, localtime() should behave as if tzset() is called. This explicit
  // call, while not always strictly required by the spec, is included to
  // guarantee that the timezone is always up-to-date. This prevents subtle
  // bugs where the TZ environment variable might change between calls.
  tzset();
  static thread_local struct tm thread_local_tm;
  return localtime_r(time, &thread_local_tm);
}
}  // extern "C"
