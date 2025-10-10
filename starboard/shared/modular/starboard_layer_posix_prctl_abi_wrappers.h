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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_PRCTL_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_PRCTL_ABI_WRAPPERS_H_

#include "starboard/export.h"

#define MUSL_PR_SET_PDEATHSIG 1
#define MUSL_PR_GET_PDEATHSIG 2
#define MUSL_PR_GET_DUMPABLE 3
#define MUSL_PR_SET_DUMPABLE 4
#define MUSL_PR_GET_KEEPCAPS 7
#define MUSL_PR_SET_KEEPCAPS 8
#define MUSL_PR_GET_TIMING 13
#define MUSL_PR_SET_TIMING 14
#define MUSL_PR_TIMING_STATISTICAL 0
#define MUSL_PR_TIMING_TIMESTAMP 1
#define MUSL_PR_SET_NAME 15
#define MUSL_PR_GET_NAME 16
#define MUSL_PR_GET_TSC 25
#define MUSL_PR_SET_TSC 26
#define MUSL_PR_TSC_ENABLE 1
#define MUSL_PR_TSC_SIGSEGV 2
#define MUSL_PR_SET_TIMERSLACK 29
#define MUSL_PR_GET_TIMERSLACK 30
#define MUSL_PR_TASK_PERF_EVENTS_DISABLE 31
#define MUSL_PR_TASK_PERF_EVENTS_ENABLE 32
#define MUSL_PR_SET_PTRACER 0x59616d61
#define MUSL_PR_SET_PTRACER_ANY (-1UL)

#ifdef __cplusplus
extern "C" {
#endif

SB_EXPORT int __abi_wrap_prctl(int op, ...);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_PRCTL_ABI_WRAPPERS_H_
