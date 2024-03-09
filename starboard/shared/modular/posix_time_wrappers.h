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

// Module Overview: POSIX time wrappers
//
// These wrappers are used in modular builds (including Evergreen). They
// are needed to handle dealing with differences in platform-specific
// toolchain types and musl-based types. In particular, the `struct timespec`
// and `struct timeval` types may contain differently sized member fields and
// when the caller is Cobalt code compiled with musl it will provide argument
// data that needs to be adjusted when passed to platform-specific system
// libraries.
//
// To do this adjustment across compilation toolchains, this file will be
// compiled using the platform-specific toolchain (as part of the Starboard
// shared library), relying on the platform-specific type definitions from
// <time.h> and <sys/time.h>. It will also define corresponding musl-compatible
// types (copying the type definitions provided in
// //third_party/musl/include/alltypes.h.in). It will provide the definition
// of "wrapper" functions for the underlying POSIX functions, with a naming
// convention of __wrap_XXX where XXX is the name of the POSIX function.
//
// For Evergreen-based modular builds, we will rely on the exported_symbols.cc
// mapping logic to map calls to the POSIX functions to the corresponding
// __wrap_XXX functions.
//
// For non-Evergreen modular builds, the Cobalt-side shared library will be
// linked with `--wrap` linker flags to remap the POSIX functions to the
// corresponding __wrap_XXX functions.
//
// In summary, taking `clock_gettime` as an example POSIX function, the
// following table shows which symbols are remaped, undefined, or defined in
// each layer for a non-Evergreen modular build:
//
// starboard.so:
//   U clock_gettime@GLIBC_2.17  (undefined reference to platform library)
//   T __wrap_clock_gettime      (concrete definition of wrapper function)
//
// cobalt.so
//   U __wrap_clock_gettime      (remapped via --wrap linker flag)
//

#ifndef STARBOARD_SHARED_MODULAR_POSIX_TIME_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_POSIX_TIME_WRAPPERS_H_

#include <stdint.h>
#include <sys/time.h>  // This should be the headers from the platform toolchain
#include <time.h>      // This should be the headers from the platform toolchain

#include "starboard/configuration.h"
#include "starboard/export.h"

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
  int64_t /* time_t */ tv_sec;
  int64_t /* suseconds_t */ tv_usec;
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
#define MUSL_CLOCK_REALTIME 0
#define MUSL_CLOCK_MONOTONIC 1
#define MUSL_CLOCK_PROCESS_CPUTIME_ID 2
#define MUSL_CLOCK_THREAD_CPUTIME_ID 3

SB_EXPORT int __wrap_clock_gettime(int /* clockid_t */ musl_clock_id,
                                   struct musl_timespec* mts);

SB_EXPORT int __wrap_gettimeofday(struct musl_timeval* mtv, void* tzp);

SB_EXPORT int64_t __wrap_time(int64_t* /* time_t* */ musl_tloc);

SB_EXPORT int64_t __wrap_timegm(struct musl_tm* musl_timeptr);

SB_EXPORT int64_t __wrap_mktime(struct musl_tm* musl_timeptr);

SB_EXPORT struct musl_tm* __wrap_gmtime_r(
    const int64_t* /* time_t* */ musl_timer,
    struct musl_tm* musl_result);

SB_EXPORT struct musl_tm* __wrap_localtime_r(
    const int64_t* /* time_t* */ musl_timer,
    struct musl_tm* musl_result);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_POSIX_TIME_WRAPPERS_H_
