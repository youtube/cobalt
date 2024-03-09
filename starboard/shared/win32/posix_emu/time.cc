// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include <sys/time.h>
#include <time.h>
#include <windows.h>
#include <limits>

#include "starboard/common/log.h"
#include "starboard/types.h"

namespace {

// Number of microseconds since Jan 1, 1601 (UTC) until Jan 1, 1970
const int64_t kWindowsToPosixDeltaUs = 11644473600000000ULL;

bool can_convert_to_filetime(int64_t posix_us) {
  int64_t windows_us = posix_us + kWindowsToPosixDeltaUs;
  return windows_us >= 0 &&
         windows_us <= (std::numeric_limits<int64_t>::max() / 10);
}

bool safe_convert_int_to_word(int in, WORD* out) {
  if (in < std::numeric_limits<WORD>::min() ||
      in > std::numeric_limits<WORD>::max()) {
    *out = std::numeric_limits<WORD>::max();
    return false;
  }
  *out = static_cast<WORD>(in);
  return true;
}

int64_t filetime_to_posix_us(const FILETIME& ft) {
  // From MSDN, FILETIME "Contains a 64-bit value representing the number of
  // 100-nanosecond intervals since January 1, 1601 (UTC)."
  ULARGE_INTEGER large_int;
  large_int.LowPart = ft.dwLowDateTime;
  large_int.HighPart = ft.dwHighDateTime;
  // Need to divide by 10 to convert 100-nanoseconds to Windows microseconds.
  int64_t windows_us = static_cast<int64_t>(large_int.QuadPart / 10);
  return windows_us - kWindowsToPosixDeltaUs;
}

FILETIME posix_us_to_filetime(int64_t posix_us) {
  SB_DCHECK(can_convert_to_filetime(posix_us))
      << "Out-of-range: Cannot convert " << posix_us
      << " microseconds to FILETIME units.";
  int64_t windows_us = posix_us + kWindowsToPosixDeltaUs;
  // Multiply by 10 to convert microseconds to 100-nanoseconds.
  uint64_t hundred_nanos = static_cast<uint64_t>(windows_us) * 10;
  ULARGE_INTEGER large_int;
  large_int.QuadPart = hundred_nanos;
  FILETIME ft;
  ft.dwLowDateTime = large_int.LowPart;
  ft.dwHighDateTime = large_int.HighPart;
  return ft;
}

bool explode(bool is_local, const time_t* posix_s, struct tm* exploded) {
  if (!posix_s || !exploded) {
    return false;
  }
  int64_t posix_us = static_cast<int64_t>(*posix_s) * 1000000;
  if (!can_convert_to_filetime(posix_us)) {
    // We are not able to convert it to FILETIME.
    return false;
  }
  const FILETIME utc_ft = posix_us_to_filetime(posix_us);
  bool success = true;
  // FILETIME in SYSTEMTIME (exploded).
  SYSTEMTIME st = {0};
  if (is_local) {
    SYSTEMTIME utc_st = {0};
    // We don't use FileTimeToLocalFileTime here, since it uses the current
    // settings for the time zone and daylight saving time. Therefore, if it is
    // daylight saving time, it will take daylight saving time into account,
    // even if the time you are converting is in standard time.
    success = FileTimeToSystemTime(&utc_ft, &utc_st) &&
              SystemTimeToTzSpecificLocalTime(nullptr, &utc_st, &st);
  } else {
    success = !!FileTimeToSystemTime(&utc_ft, &st);
  }
  if (!success) {
    return false;
  }
  exploded->tm_sec = st.wSecond;
  exploded->tm_min = st.wMinute;
  exploded->tm_hour = st.wHour;
  exploded->tm_mday = st.wDay;
  exploded->tm_mon = st.wMonth - 1;
  exploded->tm_year = st.wYear - 1900;
  exploded->tm_wday = st.wDayOfWeek;
  return true;
}

bool implode(bool is_local, struct tm* exploded, time_t* posix_s) {
  if (!exploded || !posix_s) {
    return false;
  }
  // Create the system struct representing our exploded time. It will either be
  // in local time or UTC. Fail if casting from int to WORD results in overflow.
  SYSTEMTIME st = {0};
  if (!safe_convert_int_to_word(exploded->tm_sec, &st.wSecond) ||
      !safe_convert_int_to_word(exploded->tm_min, &st.wMinute) ||
      !safe_convert_int_to_word(exploded->tm_hour, &st.wHour) ||
      !safe_convert_int_to_word(exploded->tm_mday, &st.wDay) ||
      !safe_convert_int_to_word(exploded->tm_mon + 1, &st.wMonth) ||
      !safe_convert_int_to_word(exploded->tm_year + 1900, &st.wYear) ||
      !safe_convert_int_to_word(exploded->tm_wday, &st.wDayOfWeek)) {
    return false;
  }
  FILETIME ft;
  bool success = true;
  // Ensure result is in UTC.
  if (is_local) {
    SYSTEMTIME utc_st;
    success = TzSpecificLocalTimeToSystemTime(nullptr, &st, &utc_st) &&
              SystemTimeToFileTime(&utc_st, &ft);
  } else {
    success = !!SystemTimeToFileTime(&st, &ft);
  }
  if (!success) {
    return false;
  }
  int64_t posix_us = filetime_to_posix_us(ft);
  // Narrow down to seconds, rounding negative values toward negative infinity.
  *posix_s = static_cast<time_t>(
      posix_us >= 0 ? posix_us / 1000000 : (posix_us - 1000000 + 1) / 1000000);
  return true;
}

}  // namespace

extern "C" {

int gettimeofday(struct timeval* tp, void* tzp) {
  if (tp == NULL) {
    return -1;
  }

  SYSTEMTIME st;
  GetSystemTime(&st);

  // Represents number of 100-nanosecond intervals since January 1, 1601 (UTC).
  FILETIME ft;
  SystemTimeToFileTime(&st, &ft);
  int64_t posix_us = filetime_to_posix_us(ft);

  tp->tv_sec = static_cast<time_t>(posix_us / 1000000);
  tp->tv_usec = static_cast<suseconds_t>(st.wMilliseconds) * 1000;
  return 0;
}

int clock_gettime(clockid_t clock_id, struct timespec* tp) {
  // There are only Windows implementations for realtime and monotonic clocks.
  // If CLOCK_PROCESS_CPUTIME_ID or CLOCK_THREAD_CPUTIME_ID are passed in,
  // this will return -1. Code that tries to use one of those should either
  // be able to detect and handle the -1 return value or be guarded with
  // #if SB_HAS(TIME_THREAD_NOW).
  SB_DCHECK((clock_id == CLOCK_REALTIME) || (clock_id == CLOCK_MONOTONIC) ||
            (clock_id == CLOCK_PROCESS_CPUTIME_ID) ||
            (clock_id == CLOCK_THREAD_CPUTIME_ID));

  if (tp == NULL) {
    return -1;
  }

  if (clock_id == CLOCK_REALTIME) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
      return -1;
    }
    tp->tv_sec = tv.tv_sec;
    tp->tv_nsec = static_cast<long>(tv.tv_usec) * 1000;  // NOLINT(runtime/int)
    return 0;
  } else if (clock_id == CLOCK_MONOTONIC) {
    LARGE_INTEGER counts;
    bool success;

    success = QueryPerformanceCounter(&counts);

    // "On systems that run Windows XP or later,
    // the function will always succeed and will thus never return zero."
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms644904(v=vs.85).aspx
    SB_DCHECK(success);

    LARGE_INTEGER countsPerSecond;
    success = QueryPerformanceFrequency(&countsPerSecond);
    // "On systems that run Windows XP or later,
    // the function will always succeed and will thus never return zero."
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms644905(v=vs.85).aspx
    SB_DCHECK(success);

    // An observed value of countsPerSecond on a desktop x86 machine is
    // ~2.5e6. With this frequency, it will take ~37500 days to exceed
    // 2^53, which is the mantissa precision of a double.
    // Hence, we can safely convert to a double here without losing precision.
    double result = static_cast<double>(counts.QuadPart);
    result *= (1000.0 * 1000.0) / countsPerSecond.QuadPart;
    int64_t microseconds = static_cast<int64_t>(result);
    tp->tv_sec = microseconds / 1000000;
    tp->tv_nsec = (microseconds % 1000000) * 1000;
    return 0;
  }
  return -1;
}

time_t timegm(struct tm* timeptr) {
  time_t result;
  if (!implode(/* is_local = */ false, timeptr, &result)) {
    return -1;
  }
  return result;
}

time_t sb_mktime(struct tm* timeptr) {
  time_t result;
  if (!implode(/* is_local = */ true, timeptr, &result)) {
    return -1;
  }
  return result;
}

struct tm* gmtime_r(const time_t* timer, struct tm* result) {
  // Windows `gmtime_s` function does not support times before Unix epoch, so
  // we use lower-level system calls.
  if (!explode(/* is_local = */ false, timer, result)) {
    return NULL;
  }
  return result;
}

struct tm* localtime_r(const time_t* timer, struct tm* result) {
  // Windows `localtime_s` function does not support times before Unix epoch, so
  // we use lower-level system calls.
  if (!explode(/* is_local = */ true, timer, result)) {
    return NULL;
  }
  return result;
}

}  // extern "C"

// Extra alias for C++-specific prototype in std namespace, due to #define.
namespace std {
time_t sb_mktime(struct tm* timeptr) {
  return ::sb_mktime(timeptr);
}
}  // namespace std
