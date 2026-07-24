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
#include <linux/prctl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <mutex>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/system.h"

// From third_party/musl/include/sys/prctl.h
#ifndef PR_SET_VMA
#define PR_SET_VMA 0x53564d41
#endif
#ifndef PR_SET_VMA_ANON_NAME
#define PR_SET_VMA_ANON_NAME 0
#endif

namespace {

const char k_vma_tag_file_base_name[] = "cobalt_vma_tags";
char g_vma_tag_file_path[256] = "";
std::mutex g_vma_tag_file_mutex;

void VmaTagFileCleanup() {
  // This function can be called from a signal handler, so it should not use a
  // mutex that could be held by a thread that is being interrupted. We assume
  // that g_vma_tag_file_path is written once and then only read.
  if (g_vma_tag_file_path[0]) {
    unlink(g_vma_tag_file_path);
    g_vma_tag_file_path[0] = '\0';
  }
}

void VmaTagSignalHandler(int signum) {
  VmaTagFileCleanup();
  // Re-raise signal to get default behavior (e.g. core dump).
  signal(signum, SIG_DFL);
  raise(signum);
}

}  // namespace

int musl_op_to_platform_op(int musl_op) {
  switch (musl_op) {
    case MUSL_PR_SET_PDEATHSIG:
      return PR_SET_PDEATHSIG;
    case MUSL_PR_GET_PDEATHSIG:
      return PR_GET_PDEATHSIG;
    case MUSL_PR_GET_DUMPABLE:
      return PR_GET_DUMPABLE;
    case MUSL_PR_SET_DUMPABLE:
      return PR_SET_DUMPABLE;
    case MUSL_PR_GET_KEEPCAPS:
      return PR_GET_KEEPCAPS;
    case MUSL_PR_SET_KEEPCAPS:
      return PR_SET_KEEPCAPS;
    case MUSL_PR_GET_TIMING:
      return PR_GET_TIMING;
    case MUSL_PR_SET_TIMING:
      return PR_SET_TIMING;
    case MUSL_PR_SET_NAME:
      return PR_SET_NAME;
    case MUSL_PR_GET_NAME:
      return PR_GET_NAME;
// The man-pages specify that these operations only exist on x86 platforms.
#if defined(PR_SET_TSC) && defined(PR_GET_TSC)
    case MUSL_PR_GET_TSC:
      return PR_GET_TSC;
    case MUSL_PR_SET_TSC:
      return PR_SET_TSC;
#endif  // defined(PR_SET_TSC) && defined(PR_GET_TSC)
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
    case MUSL_PR_SET_VMA:
      return PR_SET_VMA;
    default:
      SB_LOG(WARNING) << "Unknown musl prctl operation: " << musl_op;
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
      SB_LOG(WARNING) << "Unknown musl TIMING specification: " << musl_timing;
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
      SB_LOG(WARNING) << "Unknown platform timing specification: "
                      << platform_timing;
      errno = EINVAL;
      return -1;
  }
}

long musl_vma_attr_to_platform_vma_attr(long musl_vma_attr) {
  if (musl_vma_attr == MUSL_PR_SET_VMA_ANON_NAME) {
    return PR_SET_VMA_ANON_NAME;
  }

  SB_LOG(WARNING) << "Unknown musl vma addr: " << musl_vma_attr;
  errno = EINVAL;
  return -1;
}

int musl_tsc_to_platform_tsc(int musl_tsc) {
  switch (musl_tsc) {
// The man-pages specify that this operation only exist on x86 platforms.
#if defined(PR_TSC_ENABLE) && defined(PR_TSC_SIGSEGV)
    case MUSL_PR_TSC_ENABLE:
      return PR_TSC_ENABLE;
    case MUSL_PR_TSC_SIGSEGV:
      return PR_TSC_SIGSEGV;
#endif  // defined(PR_TSC_ENABLE) && defined(PR_TSC_SIGSEGV)
    default:
      SB_LOG(WARNING) << "Unknown musl tsc flag: " << musl_tsc;
      errno = EINVAL;
      return -1;
  }
}

int platform_tsc_to_musl_tsc(int platform_tsc) {
  switch (platform_tsc) {
// The man-pages specify that these operations only exist on x86 platforms.
#if defined(PR_SET_TSC) && defined(PR_GET_TSC)
    case PR_TSC_ENABLE:
      return MUSL_PR_TSC_ENABLE;
    case PR_TSC_SIGSEGV:
      return MUSL_PR_TSC_SIGSEGV;
#endif  // defined(PR_SET_TSC) && defined(PR_GET_TSC)
    default:
      SB_LOG(WARNING) << "Unknown platform tsc flag: " << platform_tsc;
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
      if (sig >= 0 && sig < NSIG) {
        ret = prctl(platform_op, sig);
      } else {
        errno = EINVAL;
      }
      break;
    }
    case PR_GET_PDEATHSIG: {
      int* sig = va_arg(args, int*);
      if (sig) {
        ret = prctl(platform_op, sig);
      } else {
        errno = EFAULT;
      }
      break;
    }
    case PR_SET_DUMPABLE: {
      long dumpable = va_arg(args, long);
      if (dumpable == 0L || dumpable == 1L) {
        ret = prctl(platform_op, dumpable);
      } else {
        errno = EINVAL;
      }
      break;
    }
    case PR_GET_DUMPABLE: {
      ret = prctl(platform_op);
      break;
    }
    case PR_SET_KEEPCAPS: {
      long keepcaps = va_arg(args, long);
      if (keepcaps == 0L || keepcaps == 1L) {
        ret = prctl(platform_op, keepcaps);
      } else {
        errno = EINVAL;
      }
      break;
    }
    case PR_GET_KEEPCAPS: {
      ret = prctl(platform_op);
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
      // should make prctl return -1 with error code EINVAL. We only support
      // |PR_TIMING_STATISTICAL|.
      if (platform_flag == PR_TIMING_STATISTICAL) {
        ret = prctl(platform_op, platform_flag);
      } else {
        errno = EINVAL;
      }
      break;
    }
    case PR_SET_NAME: {
      char* name = va_arg(args, char*);
      if (name) {
        ret = prctl(platform_op, name);
      } else {
        errno = EFAULT;
      }
      break;
    }
    // The man-pages documentation state that the buffer to hold the name should
    // be "const char name[16]", but this seems incorrect as we need to write
    // into the buffer. For PR_GET_NAME, we use just "char*" instead.
    case PR_GET_NAME: {
      char* name = va_arg(args, char*);
      if (name) {
        ret = prctl(platform_op, name);
      } else {
        errno = EFAULT;
      }
      break;
    }
// The man-pages specify that these operations only exist on x86 platforms.
#if defined(PR_SET_TSC) && defined(PR_GET_TSC)
    case PR_GET_TSC: {
      int* tsc = va_arg(args, int*);
      if (tsc) {
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
      } else {
        errno = EFAULT;
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
#endif  // defined(PR_SET_TSC) && defined(PR_GET_TSC)
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
    case PR_SET_VMA: {
      long attr = va_arg(args, long);
      unsigned long addr = va_arg(args, unsigned long);
      unsigned long size = va_arg(args, unsigned long);
      const char* val = va_arg(args, const char*);
      long platform_attr = musl_vma_attr_to_platform_vma_attr(attr);
      int result = prctl(platform_op, platform_attr, addr, size, val);
      if (result == -1 && errno == EINVAL &&
          platform_attr == MUSL_PR_SET_VMA_ANON_NAME) {
        // Kernel does not support PR_SET_VMA_ANON_NAME. Fallback to writing to
        // a file.
        {
          std::lock_guard<std::mutex> lock(g_vma_tag_file_mutex);
          if (g_vma_tag_file_path[0] == '\0') {
            char file_template[256];
            snprintf(file_template, sizeof(file_template), "/tmp/%s_%d_XXXXXX",
                     k_vma_tag_file_base_name, getpid());
            int fd = mkstemp(file_template);
            if (fd != -1) {
              starboard::strlcpy(g_vma_tag_file_path, file_template,
                                 sizeof(g_vma_tag_file_path));
              close(fd);
            } else {
              SB_LOG(ERROR) << "Failed to create VMA tag file.";
            }
          }
        }

        if (g_vma_tag_file_path[0] == '\0') {
          return result;
        }

        FILE* file = fopen(g_vma_tag_file_path, "a");
        if (file) {
          static bool cleanup_registered = false;
          if (!cleanup_registered) {
            atexit(VmaTagFileCleanup);
            // Also register signal handlers for common crash signals.
            // This is not a perfect solution as it can interfere with
            // application signal handlers and does not handle SIGKILL.
            signal(SIGSEGV, VmaTagSignalHandler);
            signal(SIGABRT, VmaTagSignalHandler);
            signal(SIGTERM, VmaTagSignalHandler);
            signal(SIGQUIT, VmaTagSignalHandler);
            signal(SIGINT, VmaTagSignalHandler);
            cleanup_registered = true;
          }
          fprintf(file, "0x%lx 0x%lx %s\n", addr, addr + size, val);
          fclose(file);
          return 0;  // Success for our fallback.
        } else {
          SB_LOG(ERROR) << "Failed to open VMA tag file: "
                        << g_vma_tag_file_path;
          // Fall through to return original error.
        }
      }
      return result;
    }
    // This default case shouldn't be reachable; if we weren't able to convert
    // the musl_op to a platform_op, this function would have returned with -1
    // already.
    default: {
      SB_LOG(WARNING)
          << "Unable to find platform operation. This default case should not "
             "be possible. Check that musl_op_to_platform_op returned -1;";
      errno = EINVAL;
    }
  }

  va_end(args);
  return ret;
}
