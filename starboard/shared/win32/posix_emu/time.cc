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

#include "starboard/common/log.h"
#include "starboard/types.h"

extern "C" {

int gettimeofday(struct timeval* tp, void* tzp) {
  if (tp == NULL) {
    return -1;
  }

  SYSTEMTIME system_time;
  GetSystemTime(&system_time);

  // Represents number of 100-nanosecond intervals since January 1, 1601 (UTC).
  FILETIME file_time;
  SystemTimeToFileTime(&system_time, &file_time);

  ULARGE_INTEGER large_int;
  large_int.LowPart = file_time.dwLowDateTime;
  large_int.HighPart = file_time.dwHighDateTime;
  // Divide by 10 to get microseconds from 100-nanosecond intervals.
  uint64_t windows_time_micros = large_int.QuadPart / 10;
  // Remove number of microseconds since Jan 1, 1601 (UTC) until Jan 1, 1970.
  uint64_t posix_time_micros = windows_time_micros - 11644473600000000ULL;

  tp->tv_sec = static_cast<time_t>(posix_time_micros / 1000000);
  tp->tv_usec = static_cast<suseconds_t>(system_time.wMilliseconds) * 1000;
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

struct tm* gmtime_r(const time_t* timer, struct tm* result) {
  if (gmtime_s(result, timer) != 0) {
    return NULL;
  }
  return result;
}

}  // extern "C"
