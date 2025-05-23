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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_TIME_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_TIME_ABI_WRAPPERS_H_

#include <stdint.h>
#include <sys/time.h>  // This should be the headers from the platform toolchain
#include <time.h>      // This should be the headers from the platform toolchain

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/log.h"

#ifdef __cplusplus
extern "C" {
#endif

// Copying types from //third_party/musl/include/alltypes.h.in
#if __ARMEB__
#define __SB_BYTE_ORDER 4321
#else
#define __SB_BYTE_ORDER 1234
#endif
#if SB_IS(ARCH_ARM64) || SB_IS(ARCH_X64)
#define __MUSL_LONG_TYPE int64_t
#else
#define __MUSL_LONG_TYPE int32_t
#endif
// Note, these structs need to ABI match the musl definitions.
struct musl_timespec {
  int64_t /* time_t */ tv_sec;
  int : 8 * (sizeof(int64_t) - sizeof(__MUSL_LONG_TYPE)) *
      (__SB_BYTE_ORDER == 4321);
  __MUSL_LONG_TYPE /* long */ tv_nsec;
  int : 8 * (sizeof(int64_t) - sizeof(__MUSL_LONG_TYPE)) *
      (__SB_BYTE_ORDER != 4321);
};

struct musl_timeval {
  int64_t tv_sec;
  int64_t tv_usec;
};

struct musl_tm {
  int32_t /* int */ tm_sec;
  int32_t /* int */ tm_min;
  int32_t /* int */ tm_hour;
  int32_t /* int */ tm_mday;
  int32_t /* int */ tm_mon;
  int32_t /* int */ tm_year;
  int32_t /* int */ tm_wday;
  int32_t /* int */ tm_yday;
  int32_t /* int */ tm_isdst;
  __MUSL_LONG_TYPE /* long */ __tm_gmtoff;
  const char* __tm_zone;
};
// Copying macro constants from //third_party/musl/include/time.h
#define MUSL_CLOCK_INVALID -1
#define MUSL_CLOCK_REALTIME 0
#define MUSL_CLOCK_MONOTONIC 1
#define MUSL_CLOCK_PROCESS_CPUTIME_ID 2
#define MUSL_CLOCK_THREAD_CPUTIME_ID 3
#define MUSL_CLOCK_MONOTONIC_RAW 4
#define MUSL_CLOCK_REALTIME_COARSE 5
#define MUSL_CLOCK_MONOTONIC_COARSE 6
#define MUSL_CLOCK_BOOTTIME 7
#define MUSL_CLOCK_REALTIME_ALARM 8
#define MUSL_CLOCK_BOOTTIME_ALARM 9
#define MUSL_CLOCK_TAI 11

inline int clock_id_to_musl_clock_id(int clock_id) {
  switch (clock_id) {
#if defined(CLOCK_REALTIME)
    case CLOCK_REALTIME:
      return MUSL_CLOCK_REALTIME;
#endif
#if defined(CLOCK_MONOTONIC)
    case CLOCK_MONOTONIC:
      return MUSL_CLOCK_MONOTONIC;
#endif
#if defined(CLOCK_PROCESS_CPUTIME_ID)
    case CLOCK_PROCESS_CPUTIME_ID:
      return MUSL_CLOCK_PROCESS_CPUTIME_ID;
#endif
#if defined(CLOCK_THREAD_CPUTIME_ID)
    case CLOCK_THREAD_CPUTIME_ID:
      return MUSL_CLOCK_THREAD_CPUTIME_ID;
#endif
#if defined(CLOCK_MONOTONIC_RAW)
    case CLOCK_MONOTONIC_RAW:
      return MUSL_CLOCK_MONOTONIC_RAW;
#endif
#if defined(CLOCK_REALTIME_COARSE)
    case CLOCK_REALTIME_COARSE:
      return MUSL_CLOCK_REALTIME_COARSE;
#endif
#if defined(CLOCK_MONOTONIC_COARSE)
    case CLOCK_MONOTONIC_COARSE:
      return MUSL_CLOCK_MONOTONIC_COARSE;
#endif
#if defined(CLOCK_BOOTTIME)
    case CLOCK_BOOTTIME:
      return MUSL_CLOCK_BOOTTIME;
#endif
#if defined(CLOCK_REALTIME_ALARM)
    case CLOCK_REALTIME_ALARM:
      return MUSL_CLOCK_REALTIME_ALARM;
#endif
#if defined(CLOCK_BOOTTIME_ALARM)
    case CLOCK_BOOTTIME_ALARM:
      return MUSL_CLOCK_BOOTTIME_ALARM;
#endif
#if defined(CLOCK_TAI)
    case CLOCK_TAI:
      return MUSL_CLOCK_TAI;
#endif
    default:
      // return a value to be treated as an invalid clock ID.
      return MUSL_CLOCK_INVALID;
  }
}

inline int musl_clock_id_to_clock_id(int musl_clock_id) {
  switch (musl_clock_id) {
#if defined(CLOCK_REALTIME)
    case MUSL_CLOCK_REALTIME:
      return CLOCK_REALTIME;
#endif
#if defined(CLOCK_MONOTONIC)
    case MUSL_CLOCK_MONOTONIC:
      return CLOCK_MONOTONIC;
#endif
#if defined(CLOCK_PROCESS_CPUTIME_ID)
    case MUSL_CLOCK_PROCESS_CPUTIME_ID:
      return CLOCK_PROCESS_CPUTIME_ID;
#endif
#if defined(CLOCK_THREAD_CPUTIME_ID)
    case MUSL_CLOCK_THREAD_CPUTIME_ID:
      return CLOCK_THREAD_CPUTIME_ID;
#endif
#if defined(CLOCK_MONOTONIC_RAW)
    case MUSL_CLOCK_MONOTONIC_RAW:
      return CLOCK_MONOTONIC_RAW;
#endif
#if defined(CLOCK_REALTIME_COARSE)
    case MUSL_CLOCK_REALTIME_COARSE:
      return CLOCK_REALTIME_COARSE;
#endif
#if defined(CLOCK_MONOTONIC_COARSE)
    case MUSL_CLOCK_MONOTONIC_COARSE:
      return CLOCK_MONOTONIC_COARSE;
#endif
#if defined(CLOCK_BOOTTIME)
    case MUSL_CLOCK_BOOTTIME:
      return CLOCK_BOOTTIME;
#endif
#if defined(CLOCK_REALTIME_ALARM)
    case MUSL_CLOCK_REALTIME_ALARM:
      return CLOCK_REALTIME_ALARM;
#endif
#if defined(CLOCK_BOOTTIME_ALARM)
    case MUSL_CLOCK_BOOTTIME_ALARM:
      return CLOCK_BOOTTIME_ALARM;
#endif
#if defined(CLOCK_TAI)
    case MUSL_CLOCK_TAI:
      return CLOCK_TAI;
#endif
    default:
      // return a value to be treated as an invalid clock ID.
      return MUSL_CLOCK_INVALID;
  }
}

#define MUSL_TIMER_ABSTIME 1

inline int musl_nanosleep_flags_to_nanosleep_flags(int musl_flags) {
  switch (musl_flags) {
    case MUSL_TIMER_ABSTIME:
      return TIMER_ABSTIME;
    default:
      return musl_flags;
  }
}

SB_EXPORT int __abi_wrap_clock_gettime(int /* clockid_t */ musl_clock_id,
                                       struct musl_timespec* mts);

SB_EXPORT int __abi_wrap_clock_nanosleep(int /* clockid_t */ musl_clock_id,
                                         int flags,
                                         const struct musl_timespec* ts,
                                         struct musl_timespec* remain);

SB_EXPORT int64_t __abi_wrap_time(int64_t* /* time_t* */ musl_tloc);

SB_EXPORT struct musl_tm* __abi_wrap_gmtime_r(
    const int64_t* /* time_t* */ musl_timer,
    struct musl_tm* musl_result);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_TIME_ABI_WRAPPERS_H_
