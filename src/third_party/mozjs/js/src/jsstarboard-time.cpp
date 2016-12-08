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

#include "jsstarboard-time.h"

#include "mozilla/Assertions.h"
#include "unicode/timezone.h"

SbTime getDSTOffset(int64_t utc_time_us) {
  // UDate is in milliseconds from the epoch.
  UDate udate = utc_time_us / kSbTimeMillisecond;

  icu::TimeZone* current_zone = icu::TimeZone::createDefault();
  int32_t raw_offset_ms, dst_offset_ms;
  UErrorCode error_code = U_ZERO_ERROR;
  current_zone->getOffset(
      udate, false /*local*/, raw_offset_ms, dst_offset_ms, error_code);
  delete current_zone;

  if (U_SUCCESS(error_code)) {
    MOZ_ASSERT(dst_offset_ms >= 0);
    return dst_offset_ms * kSbTimeMillisecond;
  }
  return 0;
}

SbTime getTZOffset() {
  icu::TimeZone* current_zone = icu::TimeZone::createDefault();
  int32_t raw_offset_ms = current_zone->getRawOffset();
  delete current_zone;
  return raw_offset_ms * kSbTimeMillisecond;
}

