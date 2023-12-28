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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_TIME_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_TIME_H_

#if defined(STARBOARD)

#include <time.h>  // For struct timespec

#ifdef __cplusplus
extern "C" {
#endif

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/time.h.html
typedef int clockid_t;
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/clock_gettime.html
int clock_gettime(clockid_t clock_id, struct timespec* tp);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // defined(STARBOARD)

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_TIME_H_
