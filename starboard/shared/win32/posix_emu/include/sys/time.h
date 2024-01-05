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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_TIME_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_TIME_H_

#if defined(STARBOARD)

#include <winsock2.h>  // For struct timeval
#undef NO_ERROR        // http://b/302733082#comment15

#ifdef __cplusplus
extern "C" {
#endif

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_time.h.html
// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/ns-winsock2-timeval
typedef long suseconds_t;  // NOLINT(runtime/int)

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/gettimeofday.html
int gettimeofday(struct timeval* tp, void* tzp);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // defined(STARBOARD)

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_TIME_H_
