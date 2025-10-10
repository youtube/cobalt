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
#include <sys/prctl.h>

extern "C" {

int __abi_wrap_prctl(int op, ...);

int prctl(int op, ...) {
  int result;
  va_list ap;
  va_start(ap, op);
  switch (op) {
    // The following commands have an long second argument.
    case PR_SET_PDEATHSIG:
    case PR_SET_DUMPABLE:
    case PR_SET_KEEPCAPS:
    case PR_SET_FPEMU:
    case PR_SET_TIMING:
    // might have to ifndef this
    case PR_SET_ENDIAN:
    case PR_SET_TSC:
    case PR_SET_PTRACER: {
      result = __abi_wrap_prctl(op, va_arg(ap, long));
      break;
    }
    // The following commands have an unsigned long second argument.
    case PR_SET_TIMERSLACK: {
      result = __abi_wrap_prctl(op, va_arg(ap, unsigned long));
      break;
    }
    // The following commands have an int* second argument.
    case PR_GET_PDEATHSIG:
    case PR_GET_FPEMU:
    // might have to ifndef this
    case PR_GET_ENDIAN:
    case PR_GET_TSC: {
      result = __abi_wrap_prctl(op, va_arg(ap, int*));
      break;
    }
    // The following commands have a char[] second arg.
    case PR_SET_NAME:
    case PR_GET_NAME: {
      result = __abi_wrap_prctl(op, va_arg(ap, char*));
      break;
    }
    // The following commands have no additional arguments;
    case PR_GET_DUMPABLE:
    case PR_GET_KEEPCAPS:
    case PR_GET_TIMING:
    case PR_GET_TIMERSLACK:
    case PR_TASK_PERF_EVENTS_DISABLE:
    case PR_TASK_PERF_EVENTS_ENABLE: {
      result = __abi_wrap_prctl(op);
      break;
    }
    // The given operation is not supported. Set errno to EINVAL and return
    // -1.
    default: {
      errno = EINVAL;
      result = -1;
    }
  }

  va_end(ap);
  return result;
}
}
