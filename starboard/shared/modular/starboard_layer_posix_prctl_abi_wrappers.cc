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

#include "starboard/shared/modular/starboard_layer_posix_prctl_abi_wrappers.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <cstring>

int musl_op_to_platform_op(int musl_op) {
  switch (musl_op) {
    case MUSL_PR_SET_PDEATHSIG:
      return PR_SET_PDEATHSIG;
    case MUSL_PR_GET_PDEATHSIG:
      return PR_GET_PDEATHSIG;
    case MUSL_PR_SET_DUMPABLE:
      return PR_SET_DUMPABLE;
    case MUSL_PR_GET_DUMPABLE:
      return PR_GET_DUMPABLE;
    case MUSL_PR_GET_KEEPCAPS:
      return PR_GET_KEEPCAPS;
    case MUSL_PR_SET_KEEPCAPS:
      return PR_SET_KEEPCAPS;
    case MUSL_PR_GET_FPEMU:
      return PR_GET_FPEMU;
    case MUSL_PR_SET_FPEMU:
      return PR_SET_FPEMU;
    case MUSL_PR_GET_TIMING:
      return PR_GET_TIMING;
    case MUSL_PR_SET_TIMING:
      return PR_SET_TIMING;
    case MUSL_PR_SET_NAME:
      return PR_SET_NAME;
    case MUSL_PR_GET_NAME:
      return PR_GET_NAME;
    case MUSL_PR_GET_ENDIAN:
      return PR_GET_ENDIAN;
    case MUSL_PR_SET_ENDIAN:
      return PR_SET_ENDIAN;
    case MUSL_PR_GET_TSC:
      return PR_GET_TSC;
    case MUSL_PR_SET_TSC:
      return PR_SET_TSC;
    case MUSL_PR_GET_TIMERSLACK:
      return PR_GET_TIMERSLACK;
    case MUSL_PR_SET_TIMERSLACK:
      return PR_SET_TIMERSLACK;
    case MUSL_PR_TASK_PERF_EVENTS_DISABLE:
      return PR_TASK_PERF_EVENTS_DISABLE;
    case MUSL_PR_TASK_PERF_EVENTS_ENABLE:
      return PR_TASK_PERF_EVENTS_ENABLE;
    case MUSL_PR_SET_PTRACER:
      return PR_SET_PTRACER;
    default:
      errno = EINVAL;
      return -1;
  }
}

int musl_fpemu_to_platform_fpemu(int musl_fpemu) {
  switch (musl_fpemu) {
    case MUSL_PR_FPEMU_NOPRINT:
      return PR_FPEMU_NOPRINT;
    case MUSL_PR_FPEMU_SIGFPE:
      return PR_FPEMU_SIGFPE;
    default:
      errno = EINVAL;
      return -1;
  }
}

int platform_fpemu_to_musl_fpemu(int platform_fpemu) {
  switch (platform_fpemu) {
    case PR_FPEMU_NOPRINT:
      return MUSL_PR_FPEMU_NOPRINT;
    case PR_FPEMU_SIGFPE:
      return MUSL_PR_FPEMU_SIGFPE;
    default:
      errno = EINVAL;
      return -1;
  }
}

long musl_timing_to_platform_timing(int musl_timing) {
  switch (musl_timing) {
    case MUSL_PR_TIMING_STATISTICAL:
      return PR_TIMING_STATISTICAL;
    case MUSL_PR_TIMING_TIMESTAMP:
      return PR_TIMING_TIMESTAMP;
    default:
      errno = EINVAL;
      return -1;
  }
}

long platform_timing_to_musl_timing(int platform_timing) {
  switch (platform_timing) {
    case PR_TIMING_STATISTICAL:
      return MUSL_PR_TIMING_STATISTICAL;
    case PR_TIMING_TIMESTAMP:
      return MUSL_PR_TIMING_TIMESTAMP;
    default:
      errno = EINVAL;
      return -1;
  }
}

int musl_endian_to_platform_endian(int musl_endian) {
  switch (musl_endian) {
    case MUSL_PR_ENDIAN_BIG:
      return PR_ENDIAN_BIG;
    case MUSL_PR_ENDIAN_LITTLE:
      return PR_ENDIAN_LITTLE;
    case MUSL_PR_ENDIAN_PPC_LITTLE:
      return PR_ENDIAN_PPC_LITTLE;
    default:
      errno = EINVAL;
      return -1;
  }
}

int platform_endian_to_musl_endian(int platform_endian) {
  switch (platform_endian) {
    case PR_ENDIAN_BIG:
      return MUSL_PR_ENDIAN_BIG;
    case PR_ENDIAN_LITTLE:
      return MUSL_PR_ENDIAN_LITTLE;
    case PR_ENDIAN_PPC_LITTLE:
      return MUSL_PR_ENDIAN_PPC_LITTLE;
    default:
      errno = EINVAL;
      return -1;
  }
}

int musl_tsc_to_platform_tsc(int musl_tsc) {
  switch (musl_tsc) {
    case MUSL_PR_TSC_ENABLE:
      return PR_TSC_ENABLE;
    case MUSL_PR_TSC_SIGSEGV:
      return PR_TSC_SIGSEGV;
    default:
      errno = EINVAL;
      return -1;
  }
}

int platform_tsc_to_musl_tsc(int platform_tsc) {
  switch (platform_tsc) {
    case PR_TSC_ENABLE:
      return MUSL_PR_TSC_ENABLE;
    case PR_TSC_SIGSEGV:
      return MUSL_PR_TSC_SIGSEGV;
    default:
      errno = EINVAL;
      return -1;
  }
}

int __abi_wrap_prctl(int op, ...) {
  int platform_op = musl_op_to_platform_op(op);

  if (platform_op == -1) {
    return -1;
  }

  va_list args;
  va_start(args, op);

  int ret = -1;

  switch (platform_op) {
    case PR_SET_PDEATHSIG: {
      long sig = va_arg(args, long);
      if (sig >= NSIG) {
        errno = EINVAL;
      } else {
        ret = prctl(platform_op, sig);
      }
      break;
    }
    case PR_GET_PDEATHSIG: {
      int* sig = va_arg(args, int*);
      if (!sig) {
        errno = EFAULT;
      } else {
        ret = prctl(platform_op, sig);
      }
      break;
    }
    case PR_SET_DUMPABLE: {
      long dumpable = va_arg(args, long);
      if (dumpable != 0L && dumpable != 1L) {
        errno = EINVAL;
      } else {
        ret = prctl(platform_op, dumpable);
      }
      break;
    }
    case PR_GET_DUMPABLE: {
      ret = prctl(platform_op);
      break;
    }
    case PR_SET_KEEPCAPS: {
      long keepcaps = va_arg(args, long);
      if (keepcaps != 0L && keepcaps != 1L) {
        errno = EINVAL;
      } else {
        ret = prctl(platform_op, keepcaps);
      }
      break;
    }
    case PR_GET_KEEPCAPS: {
      ret = prctl(platform_op);
      break;
    }
    case PR_SET_FPEMU: {
      long musl_fpemu = va_arg(args, long);
      long platform_fpemu = musl_fpemu_to_platform_fpemu(musl_fpemu);
      if (platform_fpemu != PR_FPEMU_NOPRINT &&
          platform_fpemu != PR_FPEMU_SIGFPE) {
        errno = EINVAL;
      } else {
        ret = prctl(platform_op, platform_fpemu);
      }
      break;
    }
    case PR_GET_FPEMU: {
      int* fpemu = va_arg(args, int*);
      if (!fpemu) {
        errno = EFAULT;
      } else {
        int platform_fpemu = 0;
        ret = prctl(platform_op, &platform_fpemu);
        if (ret == 0) {
          int musl_fpemu = platform_fpemu_to_musl_fpemu(platform_fpemu);
          if (musl_fpemu != -1) {
            *fpemu = musl_fpemu;
          } else {
            ret = -1;
          }
        }
      }
      break;
    }
    case PR_GET_TIMING: {
      ret = prctl(platform_op);
      if (ret != -1) {
        ret = platform_timing_to_musl_timing(ret);
      }
      break;
    }
    case PR_SET_TIMING: {
      long flag = va_arg(args, long);
      long platform_flag = musl_timing_to_platform_timing(flag);

      // According to the man-pages, PR_TIMING_TIMESTAMP is not implemented and
      // should make prctl return -1 with error code EINVAL.
      if (platform_flag == PR_TIMING_TIMESTAMP) {
        errno = EINVAL;
      } else if (platform_flag != -1) {
        ret = prctl(platform_op, platform_flag);
      }
      break;
    }
    case PR_SET_NAME: {
      char* name = va_arg(args, char*);
      if (!name) {
        errno = EFAULT;
      } else {
        ret = prctl(platform_op, name);
      }
      break;
    }
    // The man-pages documentation state that the buffer to hold the name should
    // be "const char name[16]", but this seems incorrect as we need to write
    // into the buffer. For PR_GET_NAME, we use just "char*" instead.
    case PR_GET_NAME: {
      char* name = va_arg(args, char*);
      if (!name) {
        errno = EFAULT;
      } else {
        ret = prctl(platform_op, name);
      }
      break;
    }
    case PR_GET_ENDIAN: {
      int* endian = va_arg(args, int*);
      if (!endian) {
        errno = EFAULT;
      } else {
        int platform_endian = 0;
        ret = prctl(platform_op, &platform_endian);
        if (ret == 0) {
          int musl_endian = platform_endian_to_musl_endian(platform_endian);
          if (musl_endian != -1) {
            *endian = musl_endian;
          } else {
            ret = -1;
          }
        }
      }
      break;
    }
    case PR_SET_ENDIAN: {
      long endian = va_arg(args, long);
      long platform_endian = musl_endian_to_platform_endian(endian);
      if (platform_endian != -1) {
        ret = prctl(platform_op, platform_endian);
      }
      break;
    }
    case PR_GET_TSC: {
      int* tsc = va_arg(args, int*);
      if (!tsc) {
        errno = EFAULT;
      } else {
        int platform_tsc = 0;
        ret = prctl(platform_op, &platform_tsc);
        if (ret == 0) {
          int musl_tsc = platform_tsc_to_musl_tsc(platform_tsc);
          if (musl_tsc != -1) {
            *tsc = musl_tsc;
          } else {
            ret = -1;
          }
        }
      }
      break;
    }
    case PR_SET_TSC: {
      long tsc = va_arg(args, long);
      long platform_tsc = musl_tsc_to_platform_tsc(tsc);
      if (platform_tsc != -1) {
        ret = prctl(platform_op, platform_tsc);
      }
      break;
    }
    case PR_GET_TIMERSLACK: {
      ret = prctl(platform_op);
      break;
    }
    case PR_SET_TIMERSLACK: {
      unsigned long new_slack = va_arg(args, unsigned long);
      ret = prctl(platform_op, new_slack);
      break;
    }
    case PR_TASK_PERF_EVENTS_DISABLE: {
      ret = prctl(platform_op);
      break;
    }
    case PR_TASK_PERF_EVENTS_ENABLE: {
      ret = prctl(platform_op);
      break;
    }
    case PR_SET_PTRACER: {
      long pid = va_arg(args, long);
      pid = (pid == MUSL_PR_SET_PTRACER_ANY) ? PR_SET_PTRACER_ANY : pid;
      ret = prctl(platform_op, pid);
      break;
    }
    // This default case shouldn't be reachable; if we weren't able to convert
    // the musl_op to a platform_op, this function would have returned with -1
    // already.
    default: {
      errno = EINVAL;
    }
  }

  va_end(args);
  return ret;
}
