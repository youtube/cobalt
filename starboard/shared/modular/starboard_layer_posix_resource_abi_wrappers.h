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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_RESOURCE_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_RESOURCE_ABI_WRAPPERS_H_

#include <sys/resource.h>

#include "starboard/export.h"
#include "starboard/shared/modular/starboard_layer_posix_signal_abi_wrappers.h"

#ifdef __cplusplus
extern "C" {
#endif

// MUSL specific values for specific |which| values in setpriority and
// getpriority.
#define MUSL_PRIO_PROCESS 0
#define MUSL_PRIO_PGRP 1
#define MUSL_PRIO_USER 2

// Linux specific values for the lowest and highest priorities setPriority can
// take. Since there is no hard set standard for the range of values |prio| can
// be in setpriority, we'll use the Linux range of [-20, 19].
#define LINUX_HIGHEST_PRIORITY -20
#define LINUX_LOWEST_PRIORITY 19

// MUSL defines rlim_t as an unsigned long long.
typedef unsigned long long musl_rlim_t;

struct musl_rlimit {
  musl_rlim_t musl_rlim_cur;
  musl_rlim_t musl_rlim_max;
};

// MUSL definitions for special rlim_t values
#define MUSL_RLIM_INFINITY (~0ULL)
#define MUSL_RLIM_SAVED_CUR MUSL_RLIM_INFINITY
#define MUSL_RLIM_SAVED_MAX MUSL_RLIM_INFINITY

// MUSL definitions for specific RLIMIT resource values
#define MUSL_RLIMIT_CPU 0
#define MUSL_RLIMIT_FSIZE 1
#define MUSL_RLIMIT_DATA 2
#define MUSL_RLIMIT_STACK 3
#define MUSL_RLIMIT_CORE 4
#ifndef MUSL_RLIMIT_RSS
#define MUSL_RLIMIT_RSS 5
#define MUSL_RLIMIT_NPROC 6
#define MUSL_RLIMIT_NOFILE 7
#define MUSL_RLIMIT_MEMLOCK 8
#define MUSL_RLIMIT_AS 9
#define MUSL_RLIMIT_LOCKS 10
#endif
#define MUSL_RLIMIT_SIGPENDING 11
#define MUSL_RLIMIT_MSGQUEUE 12
#define MUSL_RLIMIT_NICE 13
#define MUSL_RLIMIT_RTPRIO 14
#define MUSL_RLIMIT_RTTIME 15

#define MUSL_RLIMIT_NLIMITS 16

// 64-bit definitions for special rlim_t values
#if defined(_LARGEFILE64_SOURCE)
#define MUSL_RLIM64_INFINITY MUSL_RLIM_INFINITY
#define MUSL_RLIM64_SAVED_CUR MUSL_RLIM_SAVED_CUR
#define MUSL_RLIM64_SAVED_MAX MUSL_RLIM_SAVED_MAX
#define getrlimit64 getrlimit
#define musl_rlimit64 musl_rlimit
#define musl_rlim64_t musl_rlim_t
#endif

SB_EXPORT int __abi_wrap_getpriority(int which, musl_id_t who);

SB_EXPORT int __abi_wrap_getrlimit(int resource, struct musl_rlimit* musl_rlp);

SB_EXPORT int __abi_wrap_setpriority(int which, musl_id_t who, int prio);

#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_RESOURCE_ABI_WRAPPERS_H_
