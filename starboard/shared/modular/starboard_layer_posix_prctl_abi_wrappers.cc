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
    default:
      errno = EINVAL;
      return -1;
  }
}

int musl_unalign_flag_to_platform_unalign_flag(int musl_unalign_flag) {
  switch (musl_unalign_flag) {
    case MUSL_PR_UNALIGN_NOPRINT:
      return PR_UNALIGN_NOPRINT;
    case MUSL_PR_UNALIGN_SIGBUS:
      return PR_UNALIGN_SIGBUS;
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
    case PR_GET_UNALIGN: {
      unsigned int* bits = va_arg(args, unsigned int*);
      if (!bits) {
        errno = EFAULT;
      } else {
        ret = prctl(platform_op, bits);
      }
      break;
      case PR_SET_UNALIGN: {
        unsigned long flag = va_arg(args, unsigned long);
        unsigned long platform_flag =
            musl_unalign_flag_to_platform_unalign_flag(flag);
        if (platform_flag != -1) {
          ret = prctl(platform_op, platform_flag);
        }
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
