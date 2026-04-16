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
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <algorithm>
#include <mutex>

#include "starboard/shared/modular/starboard_layer_posix_prctl_abi_wrappers.h"
#include "starboard/system.h"

namespace {

const char kVmaTagsFileNamePrefix[] = "cobalt_vma_tags";
const char kVmaTagsFileNameSuffix[] = ".txt";

std::mutex g_vma_tags_mutex;
char g_vma_tags_file_path[256] = {0};

void VmaTagFileCleanup() {
  std::lock_guard<std::mutex> lock(g_vma_tags_mutex);
  if (g_vma_tags_file_path[0] != '\0') {
    unlink(g_vma_tags_file_path);
    g_vma_tags_file_path[0] = '\0';
  }
}

bool GetTempDir(char* path, size_t path_size) {
  if (SbSystemGetPath(kSbSystemPathTempDirectory, path, path_size)) {
    return true;
  }
  const char* tmpdir = getenv("TMPDIR");
  if (tmpdir) {
    snprintf(path, path_size, "%s", tmpdir);
    return true;
  }
  snprintf(path, path_size, "/tmp");
  return true;
}

}  // namespace

extern "C" {

int __abi_wrap_prctl(int op, ...);

int prctl(int op, ...) {
  int result;
  va_list ap;
  va_start(ap, op);
  switch (op) {
    // The following commands have a long second argument.
    case PR_SET_PDEATHSIG:
    case PR_SET_DUMPABLE:
    case PR_SET_KEEPCAPS:
    case PR_SET_TIMING:

// The man-pages specify that this operation only exist on x86 platforms.
#if defined(PR_SET_TSC)
    case PR_SET_TSC:
#endif
    case PR_SET_PTRACER: {
      result = __abi_wrap_prctl(op, va_arg(ap, long));
      break;
    }
    case MUSL_PR_SET_VMA: {
      unsigned long arg2 = va_arg(ap, unsigned long);
      unsigned long arg3 = va_arg(ap, unsigned long);
      unsigned long arg4 = va_arg(ap, unsigned long);
      unsigned long arg5 = va_arg(ap, unsigned long);
      result = __abi_wrap_prctl(op, arg2, arg3, arg4, arg5);

      if (result == -1 && arg2 == MUSL_PR_SET_VMA_ANON_NAME) {
        int saved_errno = errno;
        std::lock_guard<std::mutex> lock(g_vma_tags_mutex);
        int fd = -1;

        if (g_vma_tags_file_path[0] != '\0') {
          fd = open(g_vma_tags_file_path, O_WRONLY | O_APPEND | O_CLOEXEC);
          if (fd == -1) {
            g_vma_tags_file_path[0] = '\0';
          }
        }

        if (fd == -1 && g_vma_tags_file_path[0] == '\0') {
          char temp_dir[256];
          if (GetTempDir(temp_dir, sizeof(temp_dir))) {
            char path_template[256];
            // Include PID in naming to prevent race conditions between tests.
            snprintf(path_template, sizeof(path_template), "%s/%s_%d_XXXXXX%s",
                     temp_dir, kVmaTagsFileNamePrefix, getpid(),
                     kVmaTagsFileNameSuffix);
            fd = mkstemps(path_template, strlen(kVmaTagsFileNameSuffix));
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
        }

        if (fd != -1) {
          char buf[256];
          int len = snprintf(buf, sizeof(buf), "0x%lx 0x%lx %s\n", arg3,
                             arg3 + arg4, (const char*)arg5);
          if (len > 0) {
            size_t write_len = std::min((size_t)len, sizeof(buf) - 1);
            if (write(fd, buf, write_len) != -1) {
              close(fd);
              errno = saved_errno;
              return 0;
            }
          }
          close(fd);
        }
        errno = saved_errno;
      }
      break;
    }
    // The following commands have an unsigned long second argument.
    case PR_SET_TIMERSLACK: {
      result = __abi_wrap_prctl(op, va_arg(ap, unsigned long));
      break;
    }
    // The following commands have an int* second argument.
    case PR_GET_PDEATHSIG:
// The man-pages specify that this operation only exist on x86 platforms.
#if defined(PR_GET_TSC)
    case PR_GET_TSC:
#endif  // defined(PR_GET_TSC)
    {
      result = __abi_wrap_prctl(op, va_arg(ap, int*));
      break;
    }
    // The following commands have a char[] second argument.
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

}  // extern "C"
