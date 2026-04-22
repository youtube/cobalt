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

#include "starboard/common/log.h"

// The PR_SET_VMA* symbols are originally from
// https://android.googlesource.com/platform/bionic/+/lollipop-release/libc/private/bionic_prctl.h
// and were subsequently added to mainline Linux in Jan 2022.
//
// We conditionally define these symbols here to support older
// GNU/Linux operating systems that may not have these symbols yet.
#if !defined(PR_SET_VMA)
#define PR_SET_VMA 0x53564d41
#endif

#if !defined(PR_SET_VMA_ANON_NAME)
#define PR_SET_VMA_ANON_NAME 0
#endif

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include <map>
#include <mutex>

#ifndef __NR_memfd_create
// Source:
// https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl
#if defined(__x86_64__)
#define __NR_memfd_create 319  // x64
// Source:
// https://github.com/torvalds/linux/blob/master/include/uapi/asm-generic/unistd.h
#elif defined(__aarch64__)
#define __NR_memfd_create 279  // arm64
// Source:
// https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_32.tbl
#elif defined(__i386__)
#define __NR_memfd_create 356  // x86
// Source:
// https://github.com/torvalds/linux/blob/master/arch/arm/tools/syscall.tbl
#elif defined(__arm__)
#define __NR_memfd_create 385  // arm
#endif
#endif

#ifndef MFD_CLOEXEC
// Source:
// https://github.com/torvalds/linux/blob/master/include/uapi/linux/memfd.h
#define MFD_CLOEXEC 0x0001U
#endif

namespace {

int GetProtForAddress(unsigned long target_addr) {
  int fd = open("/proc/self/maps", O_RDONLY);
  if (fd == -1) {
    return -1;
  }

  char buf[4096];
  int prot = 0;
  bool found = false;
  ssize_t n;
  std::string line;

  // Simple line-by-line parser for /proc/self/maps
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    for (ssize_t i = 0; i < n; ++i) {
      if (buf[i] == '\n') {
        unsigned long start, end;
        char p[5];
        if (sscanf(line.c_str(), "%lx-%lx %4s", &start, &end, p) == 3) {
          if (target_addr >= start && target_addr < end) {
            if (p[0] == 'r') {
              prot |= PROT_READ;
            }
            if (p[1] == 'w') {
              prot |= PROT_WRITE;
            }
            if (p[2] == 'x') {
              prot |= PROT_EXEC;
            }
            found = true;
            break;
          }
        }
        line.clear();
      } else {
        line += buf[i];
      }
    }
    if (found) {
      break;
    }
  }
  close(fd);
  return found ? prot : -1;
}

int GetNamedFd(const char* name) {
  static std::mutex g_named_fds_mutex;
  static std::map<const std::string, int> g_named_fds;

  std::lock_guard<std::mutex> lock(g_named_fds_mutex);
  auto it = g_named_fds.find(name);
  if (it != g_named_fds.end()) {
    return it->second;
  }

  int fd = syscall(__NR_memfd_create, name, MFD_CLOEXEC);
  if (fd != -1) {
    ftruncate(fd, 1024ULL * 1024 * 1024 * 1024);
    g_named_fds[name] = fd;
  }
  return fd;
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

extern "C" {

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
      pid = (pid == (long)MUSL_PR_SET_PTRACER_ANY) ? PR_SET_PTRACER_ANY : pid;
      ret = prctl(platform_op, pid);
      break;
    }
    case PR_SET_VMA: {
      unsigned long attr = va_arg(args, unsigned long);
      unsigned long addr = va_arg(args, unsigned long);
      unsigned long size = va_arg(args, unsigned long);
      unsigned long val = va_arg(args, unsigned long);

      unsigned long platform_attr = attr;
      if (attr == MUSL_PR_SET_VMA_ANON_NAME) {
        platform_attr = PR_SET_VMA_ANON_NAME;
      }
      ret = prctl(platform_op, platform_attr, addr, size, val);

      if (ret == -1 && errno == EINVAL &&
          platform_attr == PR_SET_VMA_ANON_NAME) {
        // Fallback to memfd trick.
        int prot = GetProtForAddress(addr);
        if (prot != -1) {
          int fd = GetNamedFd((const char*)val);
          if (fd != -1) {
            // Note: We don't copy data here. This fallback is best-effort
            // and intended for use immediately after allocation.
            if (mmap((void*)addr, size, prot, MAP_FIXED | MAP_PRIVATE, fd, 0) !=
                MAP_FAILED) {
              ret = 0;
            }
          }
        }
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

  va_end(args);
  return ret;
}

}  // extern "C"
