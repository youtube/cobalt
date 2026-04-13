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
#include <fcntl.h>
#include <linux/prctl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <algorithm>
#include <mutex>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

// From third_party/musl/include/sys/prctl.h
#ifndef PR_SET_VMA
#define PR_SET_VMA 0x53564d41
#endif
#ifndef PR_SET_VMA_ANON_NAME
#define PR_SET_VMA_ANON_NAME 0
#endif

namespace {

const char kVmaTagsFileNamePrefix[] = "cobalt_vma_tags";

std::mutex g_vma_tags_mutex;
char g_vma_tags_file_path[256] = {0};

void VmaTagFileCleanup() {
  std::lock_guard<std::mutex> lock(g_vma_tags_mutex);
  if (g_vma_tags_file_path[0] != '\0') {
    unlink(g_vma_tags_file_path);
    g_vma_tags_file_path[0] = '\0';
  }
}

const char* GetTempDir() {
  const char* tmpdir = getenv("TMPDIR");
  if (!tmpdir) {
    tmpdir = "/tmp";
  }
  return tmpdir;
}

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
}  // namespace

SB_EXPORT int __abi_wrap_prctl(int option,
                               unsigned long arg2,
                               unsigned long arg3,
                               unsigned long arg4,
                               unsigned long arg5) {
  int platform_op = musl_op_to_platform_op(option);

  if (platform_op == -1) {
    return -1;
  }

  int ret = -1;

  switch (platform_op) {
    case PR_SET_PDEATHSIG: {
      long sig = static_cast<long>(arg2);
      if (sig >= 0 && sig < NSIG) {
        ret = prctl(platform_op, sig);
      } else {
        errno = EINVAL;
      }
      break;
    }
    case PR_GET_PDEATHSIG: {
      int* sig = reinterpret_cast<int*>(arg2);
      if (sig) {
        ret = prctl(platform_op, sig);
      } else {
        errno = EFAULT;
      }
      break;
    }
    case PR_SET_DUMPABLE: {
      long dumpable = static_cast<long>(arg2);
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
      long keepcaps = static_cast<long>(arg2);
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
      long flag = static_cast<long>(arg2);
      long platform_flag =
          musl_timing_to_platform_timing(static_cast<int>(flag));

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
      char* name = reinterpret_cast<char*>(arg2);
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
      char* name = reinterpret_cast<char*>(arg2);
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
      int* tsc = reinterpret_cast<int*>(arg2);
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
      long tsc = static_cast<long>(arg2);
      long platform_tsc = musl_tsc_to_platform_tsc(static_cast<int>(tsc));
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
      unsigned long new_slack = arg2;
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
      long pid = static_cast<long>(arg2);
      pid = (pid == static_cast<long>(MUSL_PR_SET_PTRACER_ANY))
                ? PR_SET_PTRACER_ANY
                : pid;
      ret = prctl(platform_op, pid);
      break;
    }
    case PR_SET_VMA: {
      ret = prctl(platform_op, arg2, arg3, arg4, arg5);
      if (ret == -1 && arg2 == PR_SET_VMA_ANON_NAME) {
        int saved_errno = errno;
        std::lock_guard<std::mutex> lock(g_vma_tags_mutex);
        int fd = -1;

        // Try to open the existing fallback file if we have a path.
        if (g_vma_tags_file_path[0] != '\0') {
          fd = open(g_vma_tags_file_path, O_WRONLY | O_APPEND | O_CLOEXEC);
          if (fd == -1) {
            // Reset the path if open fails for any reason (e.g. unlinked by
            // test).
            g_vma_tags_file_path[0] = '\0';
          }
        }

        // If no path exists or the file was deleted/inaccessible, create a new
        // one.
        if (fd == -1 && g_vma_tags_file_path[0] == '\0') {
          char path_template[256];
          snprintf(path_template, sizeof(path_template), "%s/%s_XXXXXX",
                   GetTempDir(), kVmaTagsFileNamePrefix);

          fd = mkstemp(path_template);
          if (fd != -1) {
            snprintf(g_vma_tags_file_path, sizeof(g_vma_tags_file_path), "%s",
                     path_template);
            static bool cleanup_registered = false;
            if (!cleanup_registered) {
              atexit(VmaTagFileCleanup);
              cleanup_registered = true;
            }
          }
        }

        // If we have an open file (new or existing), write the VMA info.
        if (fd != -1) {
          char buf[256];
          int len = snprintf(buf, sizeof(buf), "0x%lx 0x%lx %s\n", arg3,
                             arg3 + arg4, reinterpret_cast<const char*>(arg5));
          if (len > 0) {
            size_t write_len =
                std::min(static_cast<size_t>(len), sizeof(buf) - 1);
            if (write(fd, buf, write_len) != -1) {
              close(fd);
              errno = saved_errno;
              return 0;  // Success for our fallback.
            }
          }
          close(fd);
        }
        errno = saved_errno;
      }
      break;
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

  return ret;
}
