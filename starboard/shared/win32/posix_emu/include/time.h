// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_TIME_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_TIME_H_

// struct timespec is defined in the system time.h
#include <../ucrt/time.h>  // The Visual Studio version of this same file

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/time.h.html
typedef int clockid_t;
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3

#ifdef __cplusplus
extern "C" {
#endif

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/clock_gettime.html
int clock_gettime(clockid_t clock_id, struct timespec* tp);

// https://man7.org/linux/man-pages/man3/timegm.3.html (not officially in POSIX)
time_t timegm(struct tm* timeptr);

// https://pubs.opengroup.org/onlinepubs/009695399/functions/mktime.html
// Windows time.h defines `mktime`, but we don't want to use it.
time_t sb_mktime(struct tm* timeptr);
#undef mktime
#define mktime(t) sb_mktime(t)

// https://pubs.opengroup.org/onlinepubs/000095399/functions/gmtime_r.html
struct tm* gmtime_r(const time_t* timer, struct tm* result);

// https://pubs.opengroup.org/onlinepubs/000095399/functions/localtime_r.html
struct tm* localtime_r(const time_t* timer, struct tm* result);

#ifdef __cplusplus
}
#endif  // __cplusplus

#ifdef __cplusplus
// Extra alias for C++-specific prototype in std namespace, due to #define.
namespace std {
time_t sb_mktime(struct tm* timeptr);
}
#endif  // __cplusplus

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_TIME_H_
