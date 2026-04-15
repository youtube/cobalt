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

#include <errno.h>
#include <stdarg.h>

#include "starboard/shared/modular/starboard_layer_posix_prctl_abi_wrappers.h"

extern "C" {

int prctl(int option, ...) {
  unsigned long arg2 = 0;
  unsigned long arg3 = 0;
  unsigned long arg4 = 0;
  unsigned long arg5 = 0;

  va_list args;
  va_start(args, option);

  switch (option) {
    case MUSL_PR_SET_VMA:
      arg2 = va_arg(args, unsigned long);
      arg3 = va_arg(args, unsigned long);
      arg4 = va_arg(args, unsigned long);
      arg5 = va_arg(args, unsigned long);
      break;
    case MUSL_PR_SET_PDEATHSIG:
    case MUSL_PR_GET_PDEATHSIG:
    case MUSL_PR_GET_DUMPABLE:
    case MUSL_PR_SET_DUMPABLE:
    case MUSL_PR_GET_KEEPCAPS:
    case MUSL_PR_SET_KEEPCAPS:
    case MUSL_PR_GET_TIMING:
    case MUSL_PR_SET_TIMING:
    case MUSL_PR_SET_NAME:
    case MUSL_PR_GET_NAME:
    case MUSL_PR_GET_TSC:
    case MUSL_PR_SET_TSC:
    case MUSL_PR_GET_TIMERSLACK:
    case MUSL_PR_SET_TIMERSLACK:
    case MUSL_PR_TASK_PERF_EVENTS_DISABLE:
    case MUSL_PR_TASK_PERF_EVENTS_ENABLE:
    case MUSL_PR_SET_PTRACER:
      arg2 = va_arg(args, unsigned long);
      break;
    default:
      break;
  }
  va_end(args);

  return __abi_wrap_prctl(option, arg2, arg3, arg4, arg5);
}

}  // extern "C"
