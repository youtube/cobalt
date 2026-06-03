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

#include "cobalt/common/libc/time/icu_time_support.h"

#include <stdio.h>
#include <time.h>

#include <cmath>
#include <memory>
#include <optional>

#include "cobalt/common/icu_init/init.h"
#include "cobalt/common/libc/no_destructor.h"
#include "starboard/common/log.h"
#include "unicode/gregocal.h"
#include "unicode/timezone.h"
#include "unicode/tznames.h"
#include "unicode/unistr.h"

namespace cobalt {
namespace common {
namespace libc {
namespace time {

namespace {
// The maximum time_t value that doesn't overflow 'struct tm'.
constexpr time_t kMaxTimeValForStructTm =
    67767976233521999;  // Tue Dec 31 23:59:59 2147483647

constexpr time_t kMinTimeValForStructTm =
    -67768040609712422;  // Thu Jan  1 00:00:00 -2147481748

bool ExplodeTime(const time_t* time,
                 std::shared_ptr<const icu::TimeZone> zone,
                 struct tm* out_exploded) {
  if (!time || !out_exploded) {
    return false;
  }
  if (*time < kMinTimeValForStructTm || *time > kMaxTimeValForStructTm) {
    errno = EOVERFLOW;
    return false;
  }

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> calendar(
      new icu::GregorianCalendar(*zone, status));
  if (!calendar || U_FAILURE(status)) {
    return false;
  }

  constexpr int64_t kMaxSecondsForExactUDate =
      (1LL << std::numeric_limits<double>::digits) / 1000;
  if (*time > kMaxSecondsForExactUDate || *time < -kMaxSecondsForExactUDate) {
    // *time is too large to be converted to milliseconds in a double without
    // potential loss of precision.
    return false;
  }
  double time_ms = static_cast<double>(*time) * 1000.0;

  UDate udate = time_ms;
  calendar->setTime(udate, status);
  out_exploded->tm_year = calendar->get(UCAL_YEAR, status) - 1900;
  out_exploded->tm_mon = calendar->get(UCAL_MONTH, status);
  out_exploded->tm_mday = calendar->get(UCAL_DATE, status);
  out_exploded->tm_hour = calendar->get(UCAL_HOUR_OF_DAY, status);
  out_exploded->tm_min = calendar->get(UCAL_MINUTE, status);
  out_exploded->tm_sec = calendar->get(UCAL_SECOND, status);
  out_exploded->tm_wday = calendar->get(UCAL_DAY_OF_WEEK, status) - UCAL_SUNDAY;
  out_exploded->tm_yday = calendar->get(UCAL_DAY_OF_YEAR, status) - 1;
  if (U_FAILURE(status)) {
    // If either of the above calls have set the status to failure, the
    // conversion has failed. Note: ICU will not reset a failing status back
    // to succesful, so we only need to check it here.
    return false;
  }
  out_exploded->tm_isdst = calendar->inDaylightTime(status) ? 1 : 0;
  if (U_FAILURE(status)) {
    status = U_ZERO_ERROR;
    out_exploded->tm_isdst = -1;
  }

  if (*zone == icu::TimeZone::getUnknown()) {
    out_exploded->tm_gmtoff = 0;
    out_exploded->tm_zone = nullptr;
  } else {
    int32_t raw_offset, dst_offset;
    zone->getOffset(udate, false, raw_offset, dst_offset, status);
    if (U_SUCCESS(status)) {
      out_exploded->tm_gmtoff = (raw_offset + dst_offset) / 1000;
      bool is_daylight = (dst_offset != 0);
      out_exploded->tm_zone = tzname[is_daylight];
    } else {
      out_exploded->tm_gmtoff = 0;
      out_exploded->tm_zone = nullptr;
    }
  }

  return U_SUCCESS(status);
}

time_t ImplodeTime(struct tm* exploded,
                   std::shared_ptr<const icu::TimeZone> zone) {
  if (!exploded || !zone) {
    return -1;
  }

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> calendar(
      new icu::GregorianCalendar(*zone, status));
  if (!calendar || U_FAILURE(status)) {
    return -1;
  }

  calendar->clear();
  calendar->set(exploded->tm_year + 1900, exploded->tm_mon, exploded->tm_mday,
                exploded->tm_hour, exploded->tm_min, exploded->tm_sec);

  // Update the exploded struct with the normalized values.
  exploded->tm_year = calendar->get(UCAL_YEAR, status) - 1900;
  exploded->tm_mon = calendar->get(UCAL_MONTH, status);
  exploded->tm_mday = calendar->get(UCAL_DATE, status);
  exploded->tm_hour = calendar->get(UCAL_HOUR_OF_DAY, status);
  exploded->tm_min = calendar->get(UCAL_MINUTE, status);
  exploded->tm_sec = calendar->get(UCAL_SECOND, status);
  exploded->tm_wday = calendar->get(UCAL_DAY_OF_WEEK, status) - UCAL_SUNDAY;
  exploded->tm_yday = calendar->get(UCAL_DAY_OF_YEAR, status) - 1;
  exploded->tm_isdst = calendar->inDaylightTime(status) ? 1 : 0;

  UDate udate = calendar->getTime(status);

  if (status <= U_ZERO_ERROR) {
    return llround(udate / 1000.0);
  }

  return -1;
}

}  // namespace

IcuTimeSupport* IcuTimeSupport::GetInstance() {
  static NoDestructor<IcuTimeSupport> instance;
  cobalt::common::icu_init::EnsureInitialized();
  return instance.get();
}

void IcuTimeSupport::GetPosixTimezoneGlobals(long& out_timezone,
                                             int& out_daylight,
                                             char** out_tzname) {
  state_.GetPosixTimezoneGlobals(out_timezone, out_daylight, out_tzname);
}

bool IcuTimeSupport::ExplodeLocalTime(const time_t* time,
                                      struct tm* out_exploded) {
  return ExplodeTime(time, state_.GetTimeZone(), out_exploded);
}

bool IcuTimeSupport::ExplodeGmtTime(const time_t* time,
                                    struct tm* out_exploded) {
  bool result =
      ExplodeTime(time,
                  std::shared_ptr<const icu::TimeZone>(
                      icu::TimeZone::getGMT(), [](const icu::TimeZone*) {}),
                  out_exploded);
  if (result) {
    out_exploded->tm_zone = "UTC";
    out_exploded->tm_isdst = 0;  // UTC is never daylight savings time.
  }
  return result;
}

time_t IcuTimeSupport::ImplodeLocalTime(struct tm* exploded) {
  return ImplodeTime(exploded, state_.GetTimeZone());
}

time_t IcuTimeSupport::ImplodeGmtTime(struct tm* exploded) {
  if (exploded) {
    exploded->tm_isdst = 0;  // UTC is never daylight savings time.
  }
  return ImplodeTime(exploded,
                     std::shared_ptr<const icu::TimeZone>(
                         icu::TimeZone::getGMT(), [](const icu::TimeZone*) {}));
}

}  // namespace time
}  // namespace libc
}  // namespace common
}  // namespace cobalt
