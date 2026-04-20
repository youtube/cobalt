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
time_t mktime(struct tm* exploded) {
  auto* time_support =
      cobalt::common::libc::time::IcuTimeSupport::GetInstance();
  // Per POSIX, mktime() should behave as if tzset() is called.
  tzset();
  return time_support->ImplodeLocalTime(exploded);
}
}  // extern "C"
